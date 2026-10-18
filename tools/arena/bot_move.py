"""Return a single bot's chosen move for a given position (JSON on stdout).

Used by the web 'Play' page so a human can face any fighter (a registered bot
or a ``stockfish-skill<N>`` pseudo-bot).

    python3 -m tools.arena.bot_move --bot shark-d6 --fen "<FEN>"
"""

from __future__ import annotations

import argparse
import json
import sys
import time

import chess

from .match import bot_to_limit, open_engine
from .play_one import (PERSONALITIES, norm_personality, resolve_fighter,
                       with_movetime, with_personality)


def main(argv=None) -> int:
    p = argparse.ArgumentParser(prog="bot_move")
    p.add_argument("--bot", required=True)
    p.add_argument("--fen", required=True)
    p.add_argument("--sf-movetime", type=int, default=200)
    p.add_argument("--movetime", type=int, default=0,
                   help="override the bot's move time in ms (0 = bot default)")
    p.add_argument("--personality", default="Balanced", type=norm_personality,
                   choices=PERSONALITIES,
                   help="Shark playing style (ignored by non-Shark engines)")
    args = p.parse_args(argv)

    bot = resolve_fighter(args.bot, args.sf_movetime)
    bot = with_movetime(bot, args.movetime)
    bot = with_personality(bot, args.personality)
    try:
        board = chess.Board(args.fen)
    except ValueError as exc:
        json.dump({"error": f"bad FEN: {exc}"}, sys.stdout)
        return 1

    if board.is_game_over(claim_draw=True):
        json.dump({"move": None, "game_over": True}, sys.stdout)
        return 0

    eng = open_engine(bot)
    try:
        t0 = time.perf_counter()
        play = eng.play(board, bot_to_limit(bot))
        ms = int((time.perf_counter() - t0) * 1000)
    finally:
        eng.quit()

    move = play.move.uci() if play.move else None
    json.dump({"move": move, "bot": bot.name, "ms": ms}, sys.stdout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
