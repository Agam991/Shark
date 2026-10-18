"""Play a single game between two fighters and print the result as JSON.

Used by the web arena to run a game on demand. A fighter is either a registered
bot name, or a Stockfish pseudo-bot of the form ``stockfish-skill<N>`` (skill
0-20). Output (stdout) is a single JSON object describing the game + moves so
the browser can replay it.

    python3 -m tools.arena.play_one --white shark-d6 --black stockfish-skill3
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import sys
import threading
from pathlib import Path
from typing import Optional

from . import bots as reg
from .match import OPENING_BOOK, play_game

_SF_RE = re.compile(r"^stockfish-skill(\d+)$")


def resolve_fighter(name: str, sf_movetime: int) -> reg.Bot:
    """Return a Bot for a registered name or a stockfish-skill<N> pseudo-bot."""
    m = _SF_RE.match(name)
    if m:
        skill = max(0, min(20, int(m.group(1))))
        sf = shutil.which("stockfish") or "/opt/homebrew/bin/stockfish"
        if not Path(sf).is_file():
            raise SystemExit(f"stockfish not found at {sf}")
        return reg.Bot(name=name, engine=sf,
                       description=f"Stockfish skill {skill}",
                       options={"Skill Level": skill},
                       limits={"movetime": sf_movetime})
    bot = reg.get_bot(name)
    if bot is None:
        raise SystemExit(f"unknown fighter: {name!r}")
    if not bot.engine_exists():
        raise SystemExit(f"{name}: engine binary not built ({bot.engine_path()})")
    return bot


def with_movetime(bot: reg.Bot, ms: int) -> reg.Bot:
    """Return a copy of `bot` whose only search limit is `ms` move time.
    If ms <= 0, the bot's own limit is kept unchanged."""
    if ms and ms > 0:
        return reg.Bot(name=bot.name, engine=bot.engine,
                       description=bot.description, version=bot.version,
                       protocol=bot.protocol, options=dict(bot.options),
                       limits={"movetime": ms})
    return bot


PERSONALITIES = ("Balanced", "Aggressive", "Defensive", "Positional")


def norm_personality(name: Optional[str]) -> str:
    """Canonical personality name; anything unknown/empty falls back to Balanced."""
    for p in PERSONALITIES:
        if name and name.lower() == p.lower():
            return p
    return "Balanced"


def with_personality(bot: reg.Bot, personality: Optional[str]) -> reg.Bot:
    """Copy of `bot` with the UCI ``Personality`` option set. Engines that do not
    declare the option (Stockfish) simply ignore it (see match.open_engine)."""
    opts = dict(bot.options)
    opts["Personality"] = norm_personality(personality)
    return reg.Bot(name=bot.name, engine=bot.engine,
                   description=bot.description, version=bot.version,
                   protocol=bot.protocol, options=opts, limits=dict(bot.limits))


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="play_one")
    p.add_argument("--white", required=True)
    p.add_argument("--black", required=True)
    p.add_argument("--opening", type=int, default=0,
                   help="opening book index (default 0 = startpos)")
    p.add_argument("--sf-movetime", type=int, default=50,
                   help="ms/move for stockfish pseudo-bots")
    p.add_argument("--white-movetime", type=int, default=0,
                   help="override White's move time in ms (0 = use bot default)")
    p.add_argument("--black-movetime", type=int, default=0,
                   help="override Black's move time in ms (0 = use bot default)")
    p.add_argument("--white-personality", default="Balanced",
                   choices=PERSONALITIES, type=norm_personality)
    p.add_argument("--black-personality", default="Balanced",
                   choices=PERSONALITIES, type=norm_personality)
    p.add_argument("--max-plies", type=int, default=400)
    p.add_argument("--stream", action="store_true",
                   help="emit one JSON line per move as the game progresses")
    args = p.parse_args(argv)

    white = resolve_fighter(args.white, args.sf_movetime)
    black = resolve_fighter(args.black, args.sf_movetime)
    white = with_movetime(white, args.white_movetime)
    black = with_movetime(black, args.black_movetime)
    white = with_personality(white, args.white_personality)
    black = with_personality(black, args.black_personality)
    opening = OPENING_BOOK[args.opening % len(OPENING_BOOK)]

    on_move = None
    if args.stream:
        # Announce the pairing first so the UI can render names immediately.
        json.dump({"type": "start", "white": white.name, "black": black.name},
                  sys.stdout)
        sys.stdout.write("\n")
        sys.stdout.flush()

        def on_move(ply, san, uci, fen, ms):  # noqa: E306
            json.dump({"type": "move", "ply": ply, "san": san,
                       "uci": uci, "fen": fen, "ms": ms}, sys.stdout)
            sys.stdout.write("\n")
            sys.stdout.flush()

    # Live switching (stream mode): the web server writes JSON lines such as
    # {"side": "w", "personality": "Aggressive"} to our stdin at any time; the
    # new personality is applied right before that side's next move.
    wanted = {"w": white.options["Personality"], "b": black.options["Personality"]}
    applied = dict(wanted)
    before_move = None
    if args.stream:
        def _listen():
            for line in sys.stdin:
                try:
                    msg = json.loads(line)
                    if msg.get("side") in wanted:
                        wanted[msg["side"]] = norm_personality(msg.get("personality"))
                except (ValueError, AttributeError):
                    pass
        threading.Thread(target=_listen, daemon=True).start()

        def before_move(is_white, eng):  # noqa: E306
            side = "w" if is_white else "b"
            if wanted[side] != applied[side]:
                applied[side] = wanted[side]
                if "Personality" in eng.options:
                    eng.configure({"Personality": wanted[side]})

    r = play_game(white, black, opening=opening, max_plies=args.max_plies,
                  on_move=on_move, before_move=before_move)

    if args.stream:
        json.dump({"type": "end", "white": r.white, "black": r.black,
                   "result": r.result, "termination": r.termination,
                   "plies": r.plies}, sys.stdout)
        sys.stdout.write("\n")
        sys.stdout.flush()
        return 0

    out = {
        "white": r.white,
        "black": r.black,
        "result": r.result,
        "termination": r.termination,
        "plies": r.plies,
        "moves_uci": r.moves_uci,
        "moves_san": r.moves_san,
        "fens": r.fens,
        "times_ms": r.times_ms,
        "pgn": r.pgn,
    }
    json.dump(out, sys.stdout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
