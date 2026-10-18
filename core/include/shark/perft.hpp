#pragma once

// Perft: count leaf nodes at a given depth. The correctness oracle for movegen.

#include <cstdint>

#include "shark/board.hpp"

namespace shark {

std::uint64_t perft(Board& b, int depth);

}  // namespace shark
