#pragma once

// Board position: bitboards per (color, piece-type) + a mailbox for O(1)
// "what's on this square" lookups. Supports FEN I/O and make/unmake with a
// full undo stack (so search can explore and roll back cheaply).

#include <array>
#include <string>
#include <vector>

#include "shark/bitboard.hpp"
#include "shark/types.hpp"

namespace shark {

// A piece is (color, type). We keep a compact mailbox using this small struct.
struct Piece {
    Color color = WHITE;
    PieceType type = NO_PIECE_TYPE;
    constexpr bool is_none() const { return type == NO_PIECE_TYPE; }
};

// Info needed to unmake a move (things not recoverable from the move alone).
struct StateInfo {
    int castling;
    Square ep_square;
    int halfmove_clock;
    Piece captured;  // piece removed by this move (NO_PIECE_TYPE if none)
    std::uint64_t key;  // zobrist key before the move (restored on unmake)
};

class Board {
   public:
    Board() { set_startpos(); }
    explicit Board(const std::string& fen) { set_fen(fen); }

    void set_startpos();
    bool set_fen(const std::string& fen);
    std::string fen() const;

    // Make/unmake. make_move assumes `m` is legal (or at least pseudo-legal;
    // movegen produces legal moves). unmake_move reverses the last make_move.
    void make_move(Move m);
    void unmake_move(Move m);

    // Null move (pass the turn) — used by null-move pruning in search.
    void make_null_move();
    void unmake_null_move();

    // Queries -----------------------------------------------------------------
    Color side_to_move() const { return stm_; }
    Square ep_square() const { return ep_; }
    int castling_rights() const { return castling_; }

    Bitboard pieces() const { return occ_[WHITE] | occ_[BLACK]; }
    Bitboard pieces(Color c) const { return occ_[c]; }
    Bitboard pieces(Color c, PieceType pt) const { return bb_[c][pt]; }
    Bitboard pieces(PieceType pt) const { return bb_[WHITE][pt] | bb_[BLACK][pt]; }

    Piece piece_on(Square s) const { return mailbox_[s]; }
    Square king_square(Color c) const { return lsb(bb_[c][KING]); }
    std::uint64_t key() const { return key_; }

    // True if side `c` has any piece besides pawns/king (guards null-move
    // pruning against zugzwang in pawn endgames).
    bool has_non_pawn_material(Color c) const {
        return (bb_[c][KNIGHT] | bb_[c][BISHOP] | bb_[c][ROOK] | bb_[c][QUEEN]) != 0;
    }

    // Is square `s` attacked by any piece of color `by` (given occupancy)?
    bool is_square_attacked(Square s, Color by) const;
    bool in_check() const { return is_square_attacked(king_square(stm_), ~stm_); }

   private:
    void put_piece(Color c, PieceType pt, Square s);
    void remove_piece(Square s);
    void move_piece(Square from, Square to);

    std::array<std::array<Bitboard, PIECE_TYPE_NB>, COLOR_NB> bb_{};
    std::array<Bitboard, COLOR_NB> occ_{};
    std::array<Piece, SQ_NB> mailbox_{};

    Color stm_ = WHITE;
    int castling_ = NO_CASTLING;
    Square ep_ = SQ_NONE;
    int halfmove_clock_ = 0;
    int fullmove_number_ = 1;
    std::uint64_t key_ = 0;

    std::vector<StateInfo> history_;
};

}  // namespace shark
