"""Single-game match runner: pit two bots against each other over UCI.

Uses python-chess as the referee (move legality, draw/mate adjudication, PGN).
The engines themselves stay pure C++ / external binaries.
"""

from __future__ import annotations

import datetime as _dt
import time as _time
from dataclasses import dataclass, field
from typing import List, Optional

import chess
import chess.engine
import chess.pgn

from .bots import Bot

# A small opening book (UCI move sequences) so repeated pairings don't replay the
# same game. Each line is played once with normal colors and once reversed by the
# tournament driver, which keeps things fair.
OPENING_BOOK: List[List[str]] = [
    [],                                   # start position
    ["e2e4", "e7e5"],                     # Open game
    ["e2e4", "c7c5"],                     # Sicilian
    ["e2e4", "e7e6"],                     # French
    ["e2e4", "c7c6"],                     # Caro-Kann
    ["d2d4", "d7d5", "c2c4"],             # Queen's Gambit
    ["d2d4", "g8f6", "c2c4", "g7g6"],     # King's Indian setup
    ["g1f3", "d7d5", "g2g3"],             # Reti
    ["c2c4", "e7e5"],                     # English
    ["e2e4", "e7e5", "g1f3", "b8c6", "f1b5"],  # Ruy Lopez
]


def bot_to_limit(bot: Bot) -> chess.engine.Limit:
    """Translate a bot's single search limit into a python-chess Limit."""
    if "movetime" in bot.limits:
        return chess.engine.Limit(time=bot.limits["movetime"] / 1000.0)
    if "depth" in bot.limits:
        return chess.engine.Limit(depth=bot.limits["depth"])
    if "nodes" in bot.limits:
        return chess.engine.Limit(nodes=bot.limits["nodes"])
    raise ValueError(f"{bot.name}: no usable search limit")


def open_engine(bot: Bot) -> chess.engine.SimpleEngine:
    """Launch and configure a bot's engine, applying only declared options."""
    eng = chess.engine.SimpleEngine.popen_uci(str(bot.engine_path()))
    supported = {k: v for k, v in bot.options.items() if k in eng.options}
    if supported:
        eng.configure(supported)
    return eng


@dataclass
class GameResult:
    white: str
    black: str
    result: str          # "1-0", "0-1", "1/2-1/2"
    termination: str     # e.g. "checkmate", "stalemate", "fifty moves"
    plies: int
    pgn: str
    moves_uci: List[str] = field(default_factory=list)  # for board replay
    moves_san: List[str] = field(default_factory=list)  # for the move list
    fens: List[str] = field(default_factory=list)        # position after each ply
    times_ms: List[int] = field(default_factory=list)    # think time per ply

    def score_for(self, name: str) -> float:
        """Points (1 / 0.5 / 0) scored by `name` in this game."""
        if self.result == "1/2-1/2":
            return 0.5
        winner = self.white if self.result == "1-0" else self.black
        return 1.0 if winner == name else 0.0


def play_game(white: Bot, black: Bot,
              opening: Optional[List[str]] = None,
              max_plies: int = 400,
              white_eng: Optional[chess.engine.SimpleEngine] = None,
              black_eng: Optional[chess.engine.SimpleEngine] = None,
              on_move=None,
              before_move=None) -> GameResult:
    """Play one game. If engine handles are passed in they are reused (faster
    for tournaments); otherwise fresh processes are spawned and closed.

    ``on_move(ply, san, uci, fen)`` is called after every move, enabling live
    streaming of a game as it progresses."""
    own_white = white_eng is None
    own_black = black_eng is None
    we = white_eng or open_engine(white)
    be = black_eng or open_engine(black)

    board = chess.Board()
    game = chess.pgn.Game()
    game.headers["Event"] = "Shark Arena"
    game.headers["White"] = white.name
    game.headers["Black"] = black.name
    game.headers["Date"] = _dt.date.today().strftime("%Y.%m.%d")

    moves_uci: List[str] = []
    moves_san: List[str] = []
    fens: List[str] = []
    times_ms: List[int] = []

    def record(mv: chess.Move, ms: int = 0) -> None:
        san = board.san(mv)  # SAN must be taken BEFORE pushing
        moves_san.append(san)
        moves_uci.append(mv.uci())
        board.push(mv)
        fen = board.fen()
        fens.append(fen)
        times_ms.append(ms)
        node_ref[0] = node_ref[0].add_variation(mv)
        if on_move is not None:
            on_move(len(moves_uci), san, mv.uci(), fen, ms)

    node_ref = [game]  # mutable cell so the nested record() can advance it

    try:
        # Play forced opening moves (if legal). Book moves are instant (0 ms).
        for uci in (opening or []):
            mv = chess.Move.from_uci(uci)
            if mv in board.legal_moves:
                record(mv, 0)

        while not board.is_game_over(claim_draw=True) and board.ply() < max_plies:
            eng = we if board.turn == chess.WHITE else be
            limit = bot_to_limit(white if board.turn == chess.WHITE else black)
            if before_move is not None:
                before_move(board.turn == chess.WHITE, eng)  # e.g. live personality switch
            t0 = _time.perf_counter()
            play = eng.play(board, limit)
            ms = int((_time.perf_counter() - t0) * 1000)
            if play.move is None:
                break
            record(play.move, ms)
    finally:
        if own_white:
            we.quit()
        if own_black:
            be.quit()

    outcome = board.outcome(claim_draw=True)
    if outcome is None:
        result, termination = "1/2-1/2", "max plies / adjudicated"
    else:
        result = outcome.result()
        termination = outcome.termination.name.lower().replace("_", " ")

    game.headers["Result"] = result
    return GameResult(white.name, black.name, result, termination,
                      board.ply(), str(game), moves_uci, moves_san, fens,
                      times_ms)
