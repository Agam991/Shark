"""Calibrate a bot's Elo by laddering against Stockfish at known UCI_Elo levels.

Method: play the challenger vs Stockfish (UCI_LimitStrength + UCI_Elo) at a set
of "rungs". For each rung we get a score rate; the challenger's rating is
estimated as a games-weighted performance rating:

    perf_i = rung_elo_i + elo_from_score(score_i)      (per rung)
    estimate = sum(perf_i * games_i) / sum(games_i)

We also report the interpolated crossover (the Stockfish Elo where the challenger
scores 50%), which is the most intuitive "it plays even with X" number.

Both sides use the same movetime so the number means "strength at that time
control". Results are written to web/data/elo.json for the UI + README.

    .venv/bin/python -m tools.arena.calibrate --bot shark-blitz \\
        --games 12 --movetime 100
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import shutil
from pathlib import Path
from typing import List, Optional

from . import bots as reg
from . import rating
from .tournament import Record, play_pairing


def sf_elo_bot(elo: int, movetime: int, path: Optional[str]) -> reg.Bot:
    sf = path or shutil.which("stockfish") or "/opt/homebrew/bin/stockfish"
    if not Path(sf).is_file():
        raise SystemExit(f"stockfish not found at {sf} (brew install stockfish)")
    return reg.Bot(
        name=f"stockfish-elo{elo}",
        engine=sf,
        description=f"Stockfish (UCI_Elo {elo})",
        options={"UCI_LimitStrength": True, "UCI_Elo": elo},
        limits={"movetime": movetime},
    )


def interpolate_crossover(ladder: List[dict]) -> Optional[float]:
    """Find the opponent Elo where challenger score crosses 50%."""
    pts = sorted(ladder, key=lambda r: r["elo"])
    for a, b in zip(pts, pts[1:]):
        if (a["score"] - 0.5) * (b["score"] - 0.5) <= 0 and a["score"] != b["score"]:
            # linear interp on score between the two rungs
            frac = (0.5 - a["score"]) / (b["score"] - a["score"])
            return a["elo"] + frac * (b["elo"] - a["elo"])
    return None


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="calibrate")
    p.add_argument("--bot", default="shark-blitz")
    p.add_argument("--games", type=int, default=12, help="games per rung")
    p.add_argument("--movetime", type=int, default=100,
                   help="ms/move for BOTH sides (defines the time control)")
    p.add_argument("--rungs", type=int, nargs="*",
                   default=[1320, 1500, 1700, 1900, 2100])
    p.add_argument("--stockfish")
    p.add_argument("--max-plies", type=int, default=400)
    args = p.parse_args(argv)

    challenger = reg.get_bot(args.bot)
    if challenger is None or not challenger.engine_exists():
        raise SystemExit(f"challenger {args.bot!r} not found / not built")
    # Force the calibration time control on the challenger too (reproducible).
    challenger = reg.Bot(name=challenger.name, engine=challenger.engine,
                         description=challenger.description,
                         options=challenger.options,
                         limits={"movetime": args.movetime})

    print(f"Calibrating {challenger.name} @ {args.movetime}ms/move vs Stockfish")
    ladder: List[dict] = []
    total_perf_weight = 0.0
    total_games = 0

    for elo in sorted(args.rungs):
        opp = sf_elo_bot(elo, args.movetime, args.stockfish)
        print(f"\n  Rung SF {elo}:")
        rec = Record()
        for r in play_pairing(challenger, opp, args.games, args.max_plies):
            rec.add(r.score_for(challenger.name))
        est = rating.elo_with_error(rec.wins, rec.draws, rec.losses)
        perf = elo + rating.elo_from_score(est.score_rate)
        row = {"elo": elo, "games": rec.games, "wins": rec.wins,
               "draws": rec.draws, "losses": rec.losses,
               "score": round(est.score_rate, 4), "perf": round(perf, 1)}
        ladder.append(row)
        total_perf_weight += perf * rec.games
        total_games += rec.games
        print(f"    {challenger.name}: +{rec.wins} ={rec.draws} -{rec.losses} "
              f"(score {est.score_rate*100:.0f}%)  -> perf {perf:.0f}")

    # Robust estimate: average performance rating over rungs that are actually
    # near even (score in [0.2, 0.8]). Performance rating breaks down against
    # opponents far above/below you, so far rungs are excluded to avoid
    # inflating the number. The 50%% crossover is reported as the headline when
    # available (most intuitive: "plays evenly with Stockfish X").
    near = [r for r in ladder if 0.2 <= r["score"] <= 0.8]
    if near:
        robust = round(sum(r["perf"] * r["games"] for r in near) /
                       sum(r["games"] for r in near))
    elif total_games:
        robust = round(total_perf_weight / total_games)
    else:
        robust = None
    crossover = interpolate_crossover(ladder)
    estimate = round(crossover) if crossover is not None else robust

    out = {
        "type": "calibration",
        "bot": challenger.name,
        "generated": _dt.datetime.now().isoformat(timespec="seconds"),
        "movetime_ms": args.movetime,
        "ladder": ladder,
        "estimate_elo": estimate,
        "perf_rating": robust,
        "crossover_elo": round(crossover) if crossover is not None else None,
    }

    data_dir = reg.REPO_ROOT / "web" / "data"
    data_dir.mkdir(parents=True, exist_ok=True)
    (data_dir / "elo.json").write_text(json.dumps(out, indent=2))
    # Also persist a committed copy so the README/UI have a number after clone.
    (reg.REPO_ROOT / "docs").mkdir(exist_ok=True)
    (reg.REPO_ROOT / "docs" / "elo.json").write_text(json.dumps(out, indent=2))

    print("\n=== CALIBRATION RESULT ===")
    print(f"  {challenger.name} @ {args.movetime}ms")
    print(f"  Headline Elo (50%% crossover): {estimate}")
    print(f"  Perf rating (near-even rungs): {robust}")
    print("  wrote web/data/elo.json + docs/elo.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
