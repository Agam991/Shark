"""Round-robin tournament between Shark's four personalities.

Every pairing plays the same engine binary with a different ``Personality``
option, so any score difference comes from the personality alone. Each opening
line in the book is played with both colors, which cancels most of the
first-move advantage and the opening bias.

    python3 -m tools.arena.personality_tournament --games 60 --movetime 30

Output: a JSON file with every game plus per-pairing summaries and standings.
Pass ``--no-book`` (default) to measure the evaluation terms only, or ``--book``
to let each personality use its own opening repertoire as well.
"""

from __future__ import annotations

import argparse
import itertools
import json
import sys
import time
from collections import Counter
from pathlib import Path
from typing import Dict, List

from . import bots as reg
from . import rating
from .match import OPENING_BOOK, play_game

PERSONALITIES = ("Balanced", "Aggressive", "Defensive", "Positional")


def make_bot(personality: str, engine: str, movetime: int, book: bool,
             hash_mb: int) -> reg.Bot:
    return reg.Bot(
        name=personality,
        engine=engine,
        description=f"Shark ({personality})",
        options={"Hash": hash_mb, "Threads": 1,
                 "Personality": personality, "PersonalityBook": book},
        limits={"movetime": movetime},
    )


def play_pairing(a: str, b: str, games: int, engine: str, movetime: int,
                 book: bool, hash_mb: int, log) -> dict:
    """Play `games` games between personalities a and b (colors alternate)."""
    bot_a = make_bot(a, engine, movetime, book, hash_mb)
    bot_b = make_bot(b, engine, movetime, book, hash_mb)
    rows: List[dict] = []
    w = d = l = 0           # from a's point of view
    for g in range(games):
        opening_idx = (g // 2) % len(OPENING_BOOK)
        a_is_white = g % 2 == 0
        white, black = (bot_a, bot_b) if a_is_white else (bot_b, bot_a)
        t0 = time.perf_counter()
        r = play_game(white, black, opening=OPENING_BOOK[opening_idx])
        secs = time.perf_counter() - t0
        score_a = r.score_for(a)
        if score_a == 1.0:
            w += 1
        elif score_a == 0.0:
            l += 1
        else:
            d += 1
        rows.append({
            "game": g + 1, "white": white.name, "black": black.name,
            "opening": opening_idx, "result": r.result,
            "termination": r.termination, "plies": r.plies,
            "seconds": round(secs, 1),
        })
        log(f"  {a} vs {b}  game {g + 1}/{games}  {r.result:7s} "
            f"{r.termination:22s} {r.plies:3d} plies  (a: +{w} ={d} -{l})")
    est = rating.elo_with_error(w, d, l)
    return {"a": a, "b": b, "games": games, "wins_a": w, "draws": d,
            "losses_a": l, "score_a": round(est.score_rate, 4),
            "elo_a_minus_b": round(est.elo, 1), "margin": round(est.margin, 1),
            "rows": rows}


def standings(pairings: List[dict]) -> List[dict]:
    pts: Dict[str, float] = Counter()
    games: Dict[str, int] = Counter()
    white_pts: Dict[str, float] = Counter()
    white_games: Dict[str, int] = Counter()
    for p in pairings:
        for row in p["rows"]:
            for color, name in (("white", row["white"]), ("black", row["black"])):
                games[name] += 1
                if row["result"] == "1/2-1/2":
                    s = 0.5
                elif (row["result"] == "1-0") == (color == "white"):
                    s = 1.0
                else:
                    s = 0.0
                pts[name] += s
                if color == "white":
                    white_pts[name] += s
                    white_games[name] += 1
    out = []
    for name in PERSONALITIES:
        if not games[name]:
            continue
        rate = pts[name] / games[name]
        out.append({"personality": name, "games": games[name],
                    "points": pts[name], "score": round(rate, 4),
                    "elo_vs_field": round(rating.elo_from_score(rate), 1),
                    "score_as_white": round(white_pts[name] / max(1, white_games[name]), 4)})
    return sorted(out, key=lambda r: -r["points"])


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="personality_tournament")
    p.add_argument("--games", type=int, default=60, help="games per pairing (even)")
    p.add_argument("--movetime", type=int, default=30, help="ms per move")
    p.add_argument("--engine", default=str(reg.REPO_ROOT / "core" / "engine"))
    p.add_argument("--hash", type=int, default=16, help="hash size in MB")
    p.add_argument("--book", action="store_true",
                   help="let each personality use its opening repertoire")
    p.add_argument("--out", default=str(reg.REPO_ROOT / "docs" / "data" /
                                        "personalities.json"))
    args = p.parse_args(argv)
    if args.games % 2:
        p.error("--games must be even so both colors are played equally")

    log = lambda s: print(s, flush=True)  # noqa: E731
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    pairings: List[dict] = []
    started = time.time()
    for a, b in itertools.combinations(PERSONALITIES, 2):
        log(f"== {a} vs {b}")
        pairings.append(play_pairing(a, b, args.games, args.engine,
                                     args.movetime, args.book, args.hash, log))
        data = {
            "settings": {"games_per_pairing": args.games,
                         "movetime_ms": args.movetime, "hash_mb": args.hash,
                         "threads": 1, "opening_book": "10 lines, both colors",
                         "personality_book": args.book},
            "pairings": pairings,
            "standings": standings(pairings),
            "elapsed_seconds": round(time.time() - started),
        }
        out.write_text(json.dumps(data, indent=1))   # saved after every pairing
    log(f"Done: {sum(x['games'] for x in pairings)} games -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
