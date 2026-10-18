#pragma once

// Zobrist hashing: a 64-bit key uniquely (up to collisions) identifying a
// position, maintained incrementally across make/unmake for the transposition
// table.

#include <cstdint>

#include "shark/types.hpp"

namespace shark {

namespace zobrist {

extern std::uint64_t psq[COLOR_NB][PIECE_TYPE_NB][SQ_NB];
extern std::uint64_t castling[16];
extern std::uint64_t ep_file[8];
extern std::uint64_t side;  // XOR'd in when it's black to move

void init();

}  // namespace zobrist

}  // namespace shark
