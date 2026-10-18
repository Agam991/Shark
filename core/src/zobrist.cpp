#include "shark/zobrist.hpp"

#include <random>

namespace shark::zobrist {

std::uint64_t psq[COLOR_NB][PIECE_TYPE_NB][SQ_NB];
std::uint64_t castling[16];
std::uint64_t ep_file[8];
std::uint64_t side;

void init() {
    std::mt19937_64 rng(0x9E3779B97F4A7C15ULL);  // fixed seed = reproducible
    for (int c = 0; c < COLOR_NB; ++c)
        for (int pt = 0; pt < PIECE_TYPE_NB; ++pt)
            for (int s = 0; s < SQ_NB; ++s) psq[c][pt][s] = rng();
    for (auto& v : castling) v = rng();
    for (auto& v : ep_file) v = rng();
    side = rng();
}

}  // namespace shark::zobrist
