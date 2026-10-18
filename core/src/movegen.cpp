#include "shark/movegen.hpp"

#include <cstdlib>

namespace shark {

namespace {

void add_promotions(MoveList& list, Square from, Square to) {
    list.add(Move::make(from, to, MT_PROMOTION, QUEEN));
    list.add(Move::make(from, to, MT_PROMOTION, ROOK));
    list.add(Move::make(from, to, MT_PROMOTION, BISHOP));
    list.add(Move::make(from, to, MT_PROMOTION, KNIGHT));
}

void generate_pawn_moves(const Board& b, Color us, MoveList& list) {
    Color them = ~us;
    Bitboard pawns = b.pieces(us, PAWN);
    Bitboard empty = ~b.pieces();
    Bitboard enemies = b.pieces(them);

    const int push = (us == WHITE) ? 8 : -8;
    const Bitboard start_rank = (us == WHITE) ? RANK_2_BB : RANK_7_BB;
    const Bitboard promo_rank = (us == WHITE) ? RANK_8_BB : RANK_1_BB;

    Bitboard p = pawns;
    while (p) {
        Square from = pop_lsb(p);
        Square one = static_cast<Square>(from + push);

        // Single & double pushes.
        if (empty & square_bb(one)) {
            if (square_bb(one) & promo_rank) {
                add_promotions(list, from, one);
            } else {
                list.add(Move::make(from, one));
                if ((square_bb(from) & start_rank)) {
                    Square two = static_cast<Square>(from + 2 * push);
                    if (empty & square_bb(two))
                        list.add(Move::make(from, two));
                }
            }
        }

        // Captures (incl. promotion captures).
        Bitboard targets = pawn_attacks(us, from) & enemies;
        while (targets) {
            Square to = pop_lsb(targets);
            if (square_bb(to) & promo_rank) {
                add_promotions(list, from, to);
            } else {
                list.add(Move::make(from, to));
            }
        }

        // En passant.
        if (b.ep_square() != SQ_NONE &&
            (pawn_attacks(us, from) & square_bb(b.ep_square()))) {
            list.add(Move::make(from, b.ep_square(), MT_EN_PASSANT));
        }
    }
}

void generate_piece_moves(const Board& b, Color us, MoveList& list) {
    Bitboard own = b.pieces(us);
    Bitboard occ = b.pieces();

    Bitboard knights = b.pieces(us, KNIGHT);
    while (knights) {
        Square from = pop_lsb(knights);
        Bitboard t = knight_attacks(from) & ~own;
        while (t) list.add(Move::make(from, pop_lsb(t)));
    }

    Bitboard bishops = b.pieces(us, BISHOP);
    while (bishops) {
        Square from = pop_lsb(bishops);
        Bitboard t = bishop_attacks(from, occ) & ~own;
        while (t) list.add(Move::make(from, pop_lsb(t)));
    }

    Bitboard rooks = b.pieces(us, ROOK);
    while (rooks) {
        Square from = pop_lsb(rooks);
        Bitboard t = rook_attacks(from, occ) & ~own;
        while (t) list.add(Move::make(from, pop_lsb(t)));
    }

    Bitboard queens = b.pieces(us, QUEEN);
    while (queens) {
        Square from = pop_lsb(queens);
        Bitboard t = queen_attacks(from, occ) & ~own;
        while (t) list.add(Move::make(from, pop_lsb(t)));
    }

    Square ksq = b.king_square(us);
    Bitboard kt = king_attacks(ksq) & ~own;
    while (kt) list.add(Move::make(ksq, pop_lsb(kt)));
}

void generate_castling(const Board& b, Color us, MoveList& list) {
    Color them = ~us;
    Bitboard occ = b.pieces();
    int rights = b.castling_rights();

    Square ksq = (us == WHITE) ? E1 : E8;
    if (b.is_square_attacked(ksq, them)) return;  // can't castle out of check

    const int oo = (us == WHITE) ? WHITE_OO : BLACK_OO;
    const int ooo = (us == WHITE) ? WHITE_OOO : BLACK_OOO;

    if (rights & oo) {
        Square f = (us == WHITE) ? F1 : F8;
        Square g = (us == WHITE) ? G1 : G8;
        bool empty = !((square_bb(f) | square_bb(g)) & occ);
        if (empty && !b.is_square_attacked(f, them) &&
            !b.is_square_attacked(g, them)) {
            list.add(Move::make(ksq, g, MT_CASTLING));
        }
    }
    if (rights & ooo) {
        Square d = (us == WHITE) ? D1 : D8;
        Square c = (us == WHITE) ? C1 : C8;
        Square bsq = (us == WHITE) ? B1 : B8;
        bool empty =
            !((square_bb(d) | square_bb(c) | square_bb(bsq)) & occ);
        if (empty && !b.is_square_attacked(d, them) &&
            !b.is_square_attacked(c, them)) {
            list.add(Move::make(ksq, c, MT_CASTLING));
        }
    }
}

}  // namespace

void generate_pseudo_legal(const Board& b, MoveList& list) {
    Color us = b.side_to_move();
    generate_pawn_moves(b, us, list);
    generate_piece_moves(b, us, list);
    generate_castling(b, us, list);
}

void generate_legal(Board& b, MoveList& list) {
    MoveList pseudo;
    generate_pseudo_legal(b, pseudo);

    Color us = b.side_to_move();
    for (Move m : pseudo) {
        b.make_move(m);
        // After make_move, side flipped; the mover's king must be safe.
        if (!b.is_square_attacked(b.king_square(us), ~us)) {
            list.add(m);
        }
        b.unmake_move(m);
    }
}

}  // namespace shark
