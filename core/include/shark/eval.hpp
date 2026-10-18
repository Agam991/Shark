#pragma once

// Static evaluation. Returns a centipawn score from the SIDE-TO-MOVE's
// perspective (positive = good for the player to move) so it plugs directly
// into negamax.
//
// Uses a tapered evaluation: separate midgame/endgame piece-square tables
// interpolated by a "game phase" derived from remaining material (PeSTO-style).

#include "shark/board.hpp"

namespace shark {

// Score bounds used by search for mates / infinity.
constexpr int kValueInfinite = 32000;
constexpr int kValueMate = 31000;      // mate score (adjusted by ply in search)
constexpr int kValueMateInMaxPly = kValueMate - 1000;

void init_eval();          // builds the per-color/-square tables (call once)
int evaluate(const Board& b);

}  // namespace shark
