#pragma once

// Move generation. Strategy: generate pseudo-legal moves, then filter to legal
// by making each move and checking the mover's king isn't left in check.
// Simple and provably correct (perft-verified); can be optimized later with
// pin/check-mask generation.

#include <array>
#include <cstddef>

#include "shark/board.hpp"
#include "shark/types.hpp"

namespace shark {

// A lightweight, allocation-free move container. 256 is a safe upper bound for
// legal moves in any reachable chess position.
class MoveList {
   public:
    void add(Move m) { moves_[size_++] = m; }
    std::size_t size() const { return size_; }
    Move operator[](std::size_t i) const { return moves_[i]; }
    const Move* begin() const { return moves_.data(); }
    const Move* end() const { return moves_.data() + size_; }

   private:
    std::array<Move, 256> moves_{};
    std::size_t size_ = 0;
};

// Fill `list` with pseudo-legal moves for the side to move.
void generate_pseudo_legal(const Board& b, MoveList& list);

// Fill `list` with fully legal moves for the side to move.
void generate_legal(Board& b, MoveList& list);

}  // namespace shark
