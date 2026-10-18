"""Tournaments: Bots Royale (round-robin) and the Stockfish Gauntlet.

Games are played sequentially by default so we never oversubscribe the machine
(configurable via `concurrency`, still capped politely). Results are written as
JSON for the web dashboard, plus a combined PGN.
"""

from __future__ import annotations

import datetime as _dt
import itertools
import json
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional

from . import rating
from .bots import Bot
from .match import GameResult, open_engine, play_game, OPENING_BOOK


@dataclass
class Record:
    wins: int = 0
    draws: int = 0
    losses: int = 0

    @property
    def games(self) -> int:
        return self.wins + self.draws + self.losses

    @property
    def points(self) -> float:
        return self.wins + self.draws * 0.5

    def add(self, score: float) -> None:
        if score == 1.0:
            self.wins += 1
        elif score == 0.5:
            self.draws += 1
        else:
            self.losses += 1


def play_pairing(a: Bot, b: Bot, games: int,
                 max_plies: int = 400,
                 log=print) -> List[GameResult]:
    """Play `games` games between a and b, alternating colors and rotating the
    opening book. Engine processes are reused across the pairing for speed."""
    results: List[GameResult] = []
    ea, eb = open_engine(a), open_engine(b)
    try:
        for i in range(games):
            opening = OPENING_BOOK[i % len(OPENING_BOOK)]
            if i % 2 == 0:
                r = play_game(a, b, opening, max_plies, white_eng=ea, black_eng=eb)
            else:
                r = play_game(b, a, opening, max_plies, white_eng=eb, black_eng=ea)
            results.append(r)
            log(f"    game {i+1}/{games}: {r.white} vs {r.black} -> "
                f"{r.result} ({r.termination}, {r.plies} plies)")
    finally:
        ea.quit()
        eb.quit()
    return results


# ---- Bots Royale ------------------------------------------------------------
def royale(bots: List[Bot], games_per_pair: int = 4,
           max_plies: int = 400, log=print) -> dict:
    """Round-robin: every pair plays `games_per_pair` games."""
    records: Dict[str, Record] = {b.name: Record() for b in bots}
    cross: Dict[str, Dict[str, Record]] = {
        a.name: {b.name: Record() for b in bots if b.name != a.name}
        for a in bots
    }
    games_meta: List[dict] = []
    pgns: List[str] = []

    for a, b in itertools.combinations(bots, 2):
        log(f"  {a.name} vs {b.name}:")
        for r in play_pairing(a, b, games_per_pair, max_plies, log):
            sa = r.score_for(a.name)
            sb = r.score_for(b.name)
            records[a.name].add(sa)
            records[b.name].add(sb)
            cross[a.name][b.name].add(sa)
            cross[b.name][a.name].add(sb)
            games_meta.append({"white": r.white, "black": r.black,
                               "result": r.result, "termination": r.termination,
                               "plies": r.plies})
            pgns.append(r.pgn)

    standings = []
    for name, rec in records.items():
        est = rating.elo_with_error(rec.wins, rec.draws, rec.losses)
        standings.append({
            "name": name, "played": rec.games,
            "wins": rec.wins, "draws": rec.draws, "losses": rec.losses,
            "points": rec.points,
            "score_rate": round(est.score_rate, 4),
            "elo": round(est.elo, 1), "elo_margin": round(est.margin, 1),
        })
    standings.sort(key=lambda s: (s["points"], s["elo"]), reverse=True)
    for i, s in enumerate(standings, 1):
        s["rank"] = i

    crosstable = {
        a: {b: {"w": r.wins, "d": r.draws, "l": r.losses,
                "score": r.points} for b, r in row.items()}
        for a, row in cross.items()
    }

    return {
        "type": "royale",
        "generated": _dt.datetime.now().isoformat(timespec="seconds"),
        "bots": [b.name for b in bots],
        "settings": {"games_per_pair": games_per_pair, "max_plies": max_plies},
        "standings": standings,
        "crosstable": crosstable,
        "games": games_meta,
        "_pgn": "\n\n".join(pgns),
    }


# ---- Stockfish Gauntlet -----------------------------------------------------
def gauntlet(challenger: Bot, opponent: Bot, games: int = 20,
             elo0: float = 0.0, elo1: float = 50.0,
             max_plies: int = 400, log=print) -> dict:
    """Play `games` between challenger and opponent; report Elo + SPRT from the
    challenger's perspective."""
    rec = Record()
    games_meta: List[dict] = []
    pgns: List[str] = []

    for r in play_pairing(challenger, opponent, games, max_plies, log):
        rec.add(r.score_for(challenger.name))
        games_meta.append({"white": r.white, "black": r.black,
                           "result": r.result, "termination": r.termination,
                           "plies": r.plies})
        pgns.append(r.pgn)

    est = rating.elo_with_error(rec.wins, rec.draws, rec.losses)
    sprt = rating.sprt(rec.wins, rec.draws, rec.losses, elo0, elo1)

    return {
        "type": "gauntlet",
        "generated": _dt.datetime.now().isoformat(timespec="seconds"),
        "challenger": challenger.name,
        "opponent": opponent.name,
        "settings": {"games": games, "elo0": elo0, "elo1": elo1,
                     "max_plies": max_plies},
        "wins": rec.wins, "draws": rec.draws, "losses": rec.losses,
        "games_played": rec.games,
        "score_rate": round(est.score_rate, 4),
        "elo": round(est.elo, 1), "elo_margin": round(est.margin, 1),
        "sprt": {"llr": round(sprt.llr, 2), "lower": round(sprt.lower, 2),
                 "upper": round(sprt.upper, 2), "verdict": sprt.verdict},
        "games_meta": games_meta,
        "_pgn": "\n\n".join(pgns),
    }


def save_results(results: dict, out_dir: Path, name: str) -> Path:
    """Write results JSON (+ combined PGN) and return the JSON path."""
    out_dir.mkdir(parents=True, exist_ok=True)
    pgn = results.pop("_pgn", "")
    json_path = out_dir / f"{name}.json"
    json_path.write_text(json.dumps(results, indent=2))
    if pgn:
        (out_dir / f"{name}.pgn").write_text(pgn)
    return json_path
