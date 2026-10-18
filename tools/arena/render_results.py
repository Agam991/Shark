"""Render a personality-tournament JSON file as Markdown tables.

    python3 -m tools.arena.render_results docs/data/personalities.json

Prints standings, the pairwise table, how games ended, and the White/Black split.
The analysis text in docs/RESULTS.md is written by hand from these numbers.
"""

from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path


def render(data: dict) -> str:
    out = []
    s = data["settings"]
    games = sum(p["games"] for p in data["pairings"])
    out.append(f"{games} games, {s['movetime_ms']} ms/move, "
               f"{s['hash_mb']} MB hash, {s['threads']} thread, "
               f"personality book {'on' if s['personality_book'] else 'off'}.\n")

    out.append("**Standings**\n")
    out.append("| Rank | Personality | Games | Points | Score | Elo vs field | Score as White |")
    out.append("|---:|---|---:|---:|---:|---:|---:|")
    for i, r in enumerate(data["standings"], 1):
        out.append(f"| {i} | {r['personality']} | {r['games']} | {r['points']:g} | "
                   f"{r['score'] * 100:.1f}% | {r['elo_vs_field']:+.0f} | "
                   f"{r['score_as_white'] * 100:.1f}% |")

    out.append("\n**Head to head** (W-D-L from the first personality's side)\n")
    out.append("| Pairing | Games | W-D-L | Score | Elo difference (95%) |")
    out.append("|---|---:|---:|---:|---:|")
    for p in data["pairings"]:
        out.append(f"| {p['a']} vs {p['b']} | {p['games']} | "
                   f"{p['wins_a']}-{p['draws']}-{p['losses_a']} | "
                   f"{p['score_a'] * 100:.1f}% | "
                   f"{p['elo_a_minus_b']:+.0f} ± {p['margin']:.0f} |")

    rows = [r for p in data["pairings"] for r in p["rows"]]
    term = Counter(r["termination"] for r in rows)
    out.append("\n**How the games ended**\n")
    out.append("| Ending | Games | Share |")
    out.append("|---|---:|---:|")
    for k, v in term.most_common():
        out.append(f"| {k} | {v} | {v / len(rows) * 100:.1f}% |")

    decisive = sum(1 for r in rows if r["result"] != "1/2-1/2")
    white = sum(1 for r in rows if r["result"] == "1-0")
    black = sum(1 for r in rows if r["result"] == "0-1")
    avg = sum(r["plies"] for r in rows) / len(rows)
    out.append(f"\nDecisive games: {decisive} of {len(rows)} "
               f"({decisive / len(rows) * 100:.1f}%). White wins {white}, "
               f"Black wins {black}, draws {len(rows) - decisive}. "
               f"Average length {avg:.0f} plies.")
    return "\n".join(out)


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="render_results")
    p.add_argument("json_file")
    args = p.parse_args(argv)
    print(render(json.loads(Path(args.json_file).read_text())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
