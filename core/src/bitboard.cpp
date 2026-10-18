#include "shark/bitboard.hpp"

namespace shark {

Bitboard g_pawn_attacks[COLOR_NB][SQ_NB];
Bitboard g_knight_attacks[SQ_NB];
Bitboard g_king_attacks[SQ_NB];

namespace {

// Manhattan-safe step: move (df, dr) from square s, return SQ_NONE if off-board.
Square step(Square s, int df, int dr) {
    int f = file_of(s) + df;
    int r = rank_of(s) + dr;
    if (f < 0 || f > 7 || r < 0 || r > 7) return SQ_NONE;
    return make_square(f, r);
}

Bitboard slide(Square s, Bitboard occupied, const int (*dirs)[2], int n) {
    Bitboard attacks = 0;
    for (int i = 0; i < n; ++i) {
        Square cur = s;
        while (true) {
            cur = step(cur, dirs[i][0], dirs[i][1]);
            if (cur == SQ_NONE) break;
            attacks |= square_bb(cur);
            if (occupied & square_bb(cur)) break;  // blocked (incl. capturable)
        }
    }
    return attacks;
}

}  // namespace

void init_attacks() {
    static const int knight_deltas[8][2] = {
        {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
    static const int king_deltas[8][2] = {
        {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}};

    for (int sq = 0; sq < SQ_NB; ++sq) {
        Square s = static_cast<Square>(sq);

        Bitboard kn = 0, kg = 0;
        for (int i = 0; i < 8; ++i) {
            Square t = step(s, knight_deltas[i][0], knight_deltas[i][1]);
            if (t != SQ_NONE) kn |= square_bb(t);
            t = step(s, king_deltas[i][0], king_deltas[i][1]);
            if (t != SQ_NONE) kg |= square_bb(t);
        }
        g_knight_attacks[sq] = kn;
        g_king_attacks[sq] = kg;

        // Pawn attacks: white captures go "up" (+rank), black "down".
        Bitboard wp = 0, bp = 0;
        Square t;
        if ((t = step(s, -1, 1)) != SQ_NONE) wp |= square_bb(t);
        if ((t = step(s, 1, 1)) != SQ_NONE) wp |= square_bb(t);
        if ((t = step(s, -1, -1)) != SQ_NONE) bp |= square_bb(t);
        if ((t = step(s, 1, -1)) != SQ_NONE) bp |= square_bb(t);
        g_pawn_attacks[WHITE][sq] = wp;
        g_pawn_attacks[BLACK][sq] = bp;
    }
}

Bitboard bishop_attacks(Square s, Bitboard occupied) {
    static const int dirs[4][2] = {{1, 1}, {1, -1}, {-1, -1}, {-1, 1}};
    return slide(s, occupied, dirs, 4);
}

Bitboard rook_attacks(Square s, Bitboard occupied) {
    static const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    return slide(s, occupied, dirs, 4);
}

}  // namespace shark
