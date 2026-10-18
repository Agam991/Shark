#pragma once

// Bitboard utilities and attack generation.
//
// Sliding-piece attacks are computed on-the-fly by walking rays until a blocker.
// This is correct and simple; magic bitboards are a later perf optimization
// (they don't change results, only speed).

#include <bit>
#include <cstdint>

#include "shark/types.hpp"

namespace shark {

// ---- Single-square bit -------------------------------------------------------
constexpr Bitboard square_bb(Square s) { return Bitboard{1} << s; }

// File / rank masks (a-file = FILE_A, 1st rank = RANK_1).
constexpr Bitboard FILE_A_BB = 0x0101010101010101ULL;
constexpr Bitboard FILE_H_BB = FILE_A_BB << 7;
constexpr Bitboard RANK_1_BB = 0xFFULL;
constexpr Bitboard RANK_2_BB = RANK_1_BB << (8 * 1);
constexpr Bitboard RANK_7_BB = RANK_1_BB << (8 * 6);
constexpr Bitboard RANK_8_BB = RANK_1_BB << (8 * 7);

// ---- Bit twiddling -----------------------------------------------------------
inline int popcount(Bitboard b) { return std::popcount(b); }
inline Square lsb(Bitboard b) { return static_cast<Square>(std::countr_zero(b)); }

// Pop the least-significant bit and return its square.
inline Square pop_lsb(Bitboard& b) {
    Square s = lsb(b);
    b &= b - 1;
    return s;
}

// Shift a bitboard one step in a direction, masking off wrap-around.
template <Direction D>
constexpr Bitboard shift(Bitboard b) {
    if constexpr (D == NORTH) return b << 8;
    else if constexpr (D == SOUTH) return b >> 8;
    else if constexpr (D == EAST) return (b & ~FILE_H_BB) << 1;
    else if constexpr (D == WEST) return (b & ~FILE_A_BB) >> 1;
    else if constexpr (D == NORTH_EAST) return (b & ~FILE_H_BB) << 9;
    else if constexpr (D == NORTH_WEST) return (b & ~FILE_A_BB) << 7;
    else if constexpr (D == SOUTH_EAST) return (b & ~FILE_H_BB) >> 7;
    else if constexpr (D == SOUTH_WEST) return (b & ~FILE_A_BB) >> 9;
    else return 0;
}

// ---- Precomputed leaper attacks ---------------------------------------------
// Filled once by init_attacks(); safe to call multiple times.
extern Bitboard g_pawn_attacks[COLOR_NB][SQ_NB];
extern Bitboard g_knight_attacks[SQ_NB];
extern Bitboard g_king_attacks[SQ_NB];

void init_attacks();

inline Bitboard pawn_attacks(Color c, Square s) { return g_pawn_attacks[c][s]; }
inline Bitboard knight_attacks(Square s) { return g_knight_attacks[s]; }
inline Bitboard king_attacks(Square s) { return g_king_attacks[s]; }

// ---- Sliding attacks (ray-walked, blocker-aware) ----------------------------
Bitboard bishop_attacks(Square s, Bitboard occupied);
Bitboard rook_attacks(Square s, Bitboard occupied);
inline Bitboard queen_attacks(Square s, Bitboard occupied) {
    return bishop_attacks(s, occupied) | rook_attacks(s, occupied);
}

}  // namespace shark
