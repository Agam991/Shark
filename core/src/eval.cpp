#include "shark/eval.hpp"

#include <array>

#include <algorithm>
#include <cstdlib>

#include "shark/bitboard.hpp"
#include "shark/personality.hpp"

namespace shark {

// ============================================================================
// PeSTO (Piece-Square Tables Only) evaluation.
// Tables are indexed a8=0 .. h1=63 (rank 8 first). For a WHITE piece on LERF
// square `sq`, look up table[sq ^ 56]; for BLACK, table[sq] and negate.
// ============================================================================

namespace {

// Midgame / endgame material values, indexed by PieceType (P,N,B,R,Q,K).
constexpr int mg_value[6] = {82, 337, 365, 477, 1025, 0};
constexpr int eg_value[6] = {94, 281, 297, 512, 936, 0};

// clang-format off
constexpr int mg_pawn[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
     98, 134,  61,  95,  68, 126,  34, -11,
     -6,   7,  26,  31,  65,  56,  25, -20,
    -14,  13,   6,  21,  23,  12,  17, -23,
    -27,  -2,  -5,  12,  17,   6,  10, -25,
    -26,  -4,  -4, -10,   3,   3,  33, -12,
    -35,  -1, -20, -23, -15,  24,  38, -22,
      0,   0,   0,   0,   0,   0,   0,   0,
};
constexpr int eg_pawn[64] = {
      0,   0,   0,   0,   0,   0,   0,   0,
    178, 173, 158, 134, 147, 132, 165, 187,
     94, 100,  85,  67,  56,  53,  82,  84,
     32,  24,  13,   5,  -2,   4,  17,  17,
     13,   9,  -3,  -7,  -7,  -8,   3,  -1,
      4,   7,  -6,   1,   0,  -5,  -1,  -8,
     13,   8,   8,  10,  13,   0,   2,  -7,
      0,   0,   0,   0,   0,   0,   0,   0,
};
constexpr int mg_knight[64] = {
   -167, -89, -34, -49,  61, -97, -15,-107,
    -73, -41,  72,  36,  23,  62,   7, -17,
    -47,  60,  37,  65,  84, 129,  73,  44,
     -9,  17,  19,  53,  37,  69,  18,  22,
    -13,   4,  16,  13,  28,  19,  21,  -8,
    -23,  -9,  12,  10,  19,  17,  25, -16,
    -29, -53, -12,  -3,  -1,  18, -14, -19,
   -105, -21, -58, -33, -17, -28, -19, -23,
};
constexpr int eg_knight[64] = {
    -58, -38, -13, -28, -31, -27, -63, -99,
    -25,  -8, -25,  -2,  -9, -25, -24, -52,
    -24, -20,  10,   9,  -1,  -9, -19, -41,
    -17,   3,  22,  22,  22,  11,   8, -18,
    -18,  -6,  16,  25,  16,  17,   4, -18,
    -23,  -3,  -1,  15,  10,  -3, -20, -22,
    -42, -20, -10,  -5,  -2, -20, -23, -44,
    -29, -51, -23, -15, -22, -18, -50, -64,
};
constexpr int mg_bishop[64] = {
    -29,   4, -82, -37, -25, -42,   7,  -8,
    -26,  16, -18, -13,  30,  59,  18, -47,
    -16,  37,  43,  40,  35,  50,  37,  -2,
     -4,   5,  19,  50,  37,  37,   7,  -2,
     -6,  13,  13,  26,  34,  12,  10,   4,
      0,  15,  15,  15,  14,  27,  18,  10,
      4,  15,  16,   0,   7,  21,  33,   1,
    -33,  -3, -14, -21, -13, -12, -39, -21,
};
constexpr int eg_bishop[64] = {
    -14, -21, -11,  -8,  -7,  -9, -17, -24,
     -8,  -4,   7, -12,  -3, -13,  -4, -14,
      2,  -8,   0,  -1,  -2,   6,   0,   4,
     -3,   9,  12,   9,  14,  10,   3,   2,
     -6,   3,  13,  19,   7,  10,  -3,  -9,
    -12,  -3,   8,  10,  13,   3,  -7, -15,
    -14, -18,  -7,  -1,   4,  -9, -15, -27,
    -23,  -9, -23,  -5,  -9, -16,  -5, -17,
};
constexpr int mg_rook[64] = {
     32,  42,  32,  51,  63,   9,  31,  43,
     27,  32,  58,  62,  80,  67,  26,  44,
     -5,  19,  26,  36,  17,  45,  61,  16,
    -24, -11,   7,  26,  24,  35,  -8, -20,
    -36, -26, -12,  -1,   9,  -7,   6, -23,
    -45, -25, -16, -17,   3,   0,  -5, -33,
    -44, -16, -20,  -9,  -1,  11,  -6, -71,
    -19, -13,   1,  17,  16,   7, -37, -26,
};
constexpr int eg_rook[64] = {
     13,  10,  18,  15,  12,  12,   8,   5,
     11,  13,  13,  11,  -3,   3,   8,   3,
      7,   7,   7,   5,   4,  -3,  -5,  -3,
      4,   3,  13,   1,   2,   1,  -1,   2,
      3,   5,   8,   4,  -5,  -6,  -8, -11,
     -4,   0,  -5,  -1,  -7, -12,  -8, -16,
     -6,  -6,   0,   2,  -9,  -9, -11,  -3,
     -9,   2,   3,  -1,  -5, -13,   4, -20,
};
constexpr int mg_queen[64] = {
    -28,   0,  29,  12,  59,  44,  43,  45,
    -24, -39,  -5,   1, -16,  57,  28,  54,
    -13, -17,   7,   8,  29,  56,  47,  57,
    -27, -27, -16, -16,  -1,  17,  -2,   1,
     -9, -26,  -9, -10,  -2,  -4,   3,  -3,
    -14,   2, -11,  -2,  -5,   2,  14,   5,
    -35,  -8,  11,   2,   8,  15,  -3,   1,
     -1, -18,  -9,  10, -15, -25, -31, -50,
};
constexpr int eg_queen[64] = {
     -9,  22,  22,  27,  27,  19,  10,  20,
    -17,  20,  32,  41,  58,  25,  30,   0,
    -20,   6,   9,  49,  47,  35,  19,   9,
      3,  22,  24,  45,  57,  40,  57,  36,
    -18,  28,  19,  47,  31,  34,  39,  23,
    -16, -27,  15,   6,   9,  17,  10,   5,
    -22, -23, -30, -16, -16, -23, -36, -32,
    -33, -28, -22, -43,  -5, -32, -20, -41,
};
constexpr int mg_king[64] = {
    -65,  23,  16, -15, -56, -34,   2,  13,
     29,  -1, -20,  -7,  -8,  -4, -38, -29,
     -9,  24,   2, -16, -20,   6,  22, -22,
    -17, -20, -12, -27, -30, -25, -14, -36,
    -49,  -1, -27, -39, -46, -44, -33, -51,
    -14, -14, -22, -46, -44, -30, -15, -27,
      1,   7,  -8, -64, -43, -16,   9,   8,
    -15,  36,  12, -54,   8, -28,  24,  14,
};
constexpr int eg_king[64] = {
    -74, -35, -18, -18, -11,  15,   4, -17,
    -12,  17,  14,  17,  17,  38,  23,  11,
     10,  17,  23,  15,  20,  45,  44,  13,
     -8,  22,  24,  27,  26,  33,  26,   3,
    -18,  -4,  21,  24,  27,  23,   9, -11,
    -19,  -3,  11,  21,  23,  16,   7,  -9,
    -27, -11,   4,  13,  14,   4,  -5, -17,
    -53, -34, -21, -11, -28, -14, -24, -43,
};
// clang-format on

const int* mg_tables[6] = {mg_pawn, mg_knight, mg_bishop,
                           mg_rook, mg_queen, mg_king};
const int* eg_tables[6] = {eg_pawn, eg_knight, eg_bishop,
                           eg_rook, eg_queen, eg_king};

// Combined (material + PST) tables, precomputed per color and square.
int g_mg[COLOR_NB][6][64];
int g_eg[COLOR_NB][6][64];

// Phase weights per piece type (P,N,B,R,Q,K).
constexpr int kPhaseInc[6] = {0, 1, 1, 2, 4, 0};
constexpr int kMaxPhase = 24;  // 4*knight/bishop + 4*rook + 2*queen equivalents


// ---------------------------------------------------------------------------
// Personality terms (Aggressive / Defensive / Positional). Skipped entirely for
// Balanced, so the default evaluation is bit-for-bit the original PeSTO score.
// ---------------------------------------------------------------------------
constexpr Bitboard kCenter = (Bitboard{1} << D4) | (Bitboard{1} << E4) |
                             (Bitboard{1} << D5) | (Bitboard{1} << E5);
constexpr int kAttackUnits[6] = {0, 2, 2, 3, 5, 0};          // P,N,B,R,Q,K
constexpr int kPassedBase[8] = {0, 5, 10, 20, 35, 60, 100, 0}; // by relative rank

inline Bitboard file_bb(int f) { return FILE_A_BB << f; }
inline Bitboard adjacent_files(int f) {
    Bitboard b = 0;
    if (f > 0) b |= file_bb(f - 1);
    if (f < 7) b |= file_bb(f + 1);
    return b;
}
// All squares strictly in front of rank r from color c's point of view.
inline Bitboard ranks_ahead(Color c, int r) {
    if (c == WHITE) return r >= 7 ? 0 : (~Bitboard{0}) << (8 * (r + 1));
    return r <= 0 ? 0 : ((Bitboard{1} << (8 * r)) - 1);
}
inline int rel_rank(Color c, Square s) {
    return c == WHITE ? rank_of(s) : 7 - rank_of(s);
}
inline int cheb(Square a, Square b) {
    return std::max(std::abs(file_of(a) - file_of(b)),
                    std::abs(rank_of(a) - rank_of(b)));
}
inline Bitboard pawn_attack_set(Color c, Bitboard pawns) {
    return c == WHITE ? (shift<NORTH_EAST>(pawns) | shift<NORTH_WEST>(pawns))
                      : (shift<SOUTH_EAST>(pawns) | shift<SOUTH_WEST>(pawns));
}

struct SideTerms {
    int mg = 0;      // king-related terms, scaled by game phase later
    int all = 0;     // phase-independent terms
    int danger = 0;  // attack danger THIS side poses to the enemy king
};

SideTerms side_terms(const Board& b, Color c, const EvalWeights& w) {
    const Color e = ~c;
    const Bitboard occ = b.pieces();
    const Bitboard own = b.pieces(c);
    const Bitboard own_p = b.pieces(c, PAWN);
    const Bitboard en_p = b.pieces(e, PAWN);
    const Square eks = b.king_square(e);
    const Square oks = b.king_square(c);
    const Bitboard ezone = king_attacks(eks) | (Bitboard{1} << eks);

    int units = 0, attackers = 0, mob = 0, prox = 0, ctr = 0;
    SideTerms t;

    for (int pt = KNIGHT; pt <= QUEEN; ++pt) {
        Bitboard bb = b.pieces(c, static_cast<PieceType>(pt));
        if (pt == BISHOP && popcount(bb) >= 2) t.all += w.bishop_pair;
        while (bb) {
            Square s = pop_lsb(bb);
            Bitboard a = pt == KNIGHT   ? knight_attacks(s)
                         : pt == BISHOP ? bishop_attacks(s, occ)
                         : pt == ROOK   ? rook_attacks(s, occ)
                                        : queen_attacks(s, occ);
            mob += popcount(a & ~own);
            ctr += popcount(a & kCenter);
            prox += 7 - cheb(s, eks);
            if (a & ezone) { ++attackers; units += kAttackUnits[pt]; }

            if (pt == KNIGHT) {
                int rr = rel_rank(c, s);
                bool supported = (pawn_attacks(e, s) & own_p) != 0;
                bool safe = (adjacent_files(file_of(s)) & ranks_ahead(c, rank_of(s)) & en_p) == 0;
                if (rr >= 3 && rr <= 5 && supported && safe) t.all += w.outpost;
            } else if (pt == ROOK) {
                Bitboard f = file_bb(file_of(s));
                if (!(f & own_p)) t.all += (f & en_p) ? w.rook_semi : w.rook_open;
            }
        }
    }
    t.all += w.mobility * mob + w.center * (ctr + popcount(pawn_attack_set(c, own_p) & kCenter));

    // Pawn structure.
    Bitboard pb = own_p;
    while (pb) {
        Square s = pop_lsb(pb);
        int f = file_of(s), rr = rel_rank(c, s);
        Bitboard front = (file_bb(f) | adjacent_files(f)) & ranks_ahead(c, rank_of(s));
        if (!(front & en_p)) t.all += kPassedBase[rr] * w.passed_pct / 100;
        if (rr >= 3) t.all += w.space * (rr - 2);
    }
    for (int f = 0; f < 8; ++f) {
        int n = popcount(own_p & file_bb(f));
        if (n > 1) t.all -= w.doubled * (n - 1);
        if (n > 0 && !(own_p & adjacent_files(f))) t.all -= w.isolated * n;
    }

    // King attack / king shield.
    t.danger = attackers >= 2 ? std::min(units * attackers, 40) : 0;
    Bitboard kz = king_attacks(oks) | (Bitboard{1} << oks);
    Bitboard front = c == WHITE ? (shift<NORTH>(kz) | shift<NORTH>(shift<NORTH>(kz)))
                                : (shift<SOUTH>(kz) | shift<SOUTH>(shift<SOUTH>(kz)));
    int shield = popcount(own_p & front);
    t.mg = w.king_attack * t.danger + w.king_prox * prox + w.shield * shield;
    return t;
}

// White-minus-black centipawns for the active personality.
int personality_score(const Board& b, const EvalWeights& w, int phase) {
    SideTerms tw = side_terms(b, WHITE, w);
    SideTerms tb = side_terms(b, BLACK, w);
    int mg_w = tw.mg - w.king_exposure * tb.danger;
    int mg_b = tb.mg - w.king_exposure * tw.danger;
    return (mg_w - mg_b) * phase / kMaxPhase + (tw.all - tb.all);
}

}  // namespace

void init_eval() {
    for (int pt = PAWN; pt <= KING; ++pt) {
        for (int sq = 0; sq < 64; ++sq) {
            // White: mirror square vertically (table is a8-first).
            g_mg[WHITE][pt][sq] = mg_value[pt] + mg_tables[pt][sq ^ 56];
            g_eg[WHITE][pt][sq] = eg_value[pt] + eg_tables[pt][sq ^ 56];
            // Black: table as-is.
            g_mg[BLACK][pt][sq] = mg_value[pt] + mg_tables[pt][sq];
            g_eg[BLACK][pt][sq] = eg_value[pt] + eg_tables[pt][sq];
        }
    }
}

int evaluate(const Board& b) {
    int mg[COLOR_NB] = {0, 0};
    int eg[COLOR_NB] = {0, 0};
    int phase = 0;

    for (int c = WHITE; c <= BLACK; ++c) {
        for (int pt = PAWN; pt <= KING; ++pt) {
            Bitboard bb = b.pieces(static_cast<Color>(c),
                                   static_cast<PieceType>(pt));
            while (bb) {
                Square s = pop_lsb(bb);
                mg[c] += g_mg[c][pt][s];
                eg[c] += g_eg[c][pt][s];
                phase += kPhaseInc[pt];
            }
        }
    }

    int mg_score = mg[WHITE] - mg[BLACK];
    int eg_score = eg[WHITE] - eg[BLACK];

    if (phase > kMaxPhase) phase = kMaxPhase;  // early promotions can exceed
    int score = (mg_score * phase + eg_score * (kMaxPhase - phase)) / kMaxPhase;

    Personality pers = current_personality();
    if (pers != Personality::Balanced)
        score += personality_score(b, personality_weights(pers), phase);

    // Return from side-to-move perspective.
    return (b.side_to_move() == WHITE) ? score : -score;
}

}  // namespace shark
