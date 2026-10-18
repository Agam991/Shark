#pragma once

// Core value types for the Shark engine.
//
// Board mapping: LERF (Little-Endian Rank-File).
//   square = rank * 8 + file,  A1 = 0, H1 = 7, A8 = 56, H8 = 63.
//   file_of(sq) = sq & 7  (0 = a-file)
//   rank_of(sq) = sq >> 3  (0 = 1st rank)

#include <cstdint>
#include <string>

namespace shark {

using Bitboard = std::uint64_t;

// ---- Colors -----------------------------------------------------------------
enum Color : int { WHITE = 0, BLACK = 1, COLOR_NB = 2 };

constexpr Color operator~(Color c) {
    return static_cast<Color>(c ^ BLACK);  // flip side to move
}

// ---- Piece types ------------------------------------------------------------
enum PieceType : int {
    PAWN = 0, KNIGHT, BISHOP, ROOK, QUEEN, KING,
    PIECE_TYPE_NB = 6,
    NO_PIECE_TYPE = 6
};

// ---- Squares ----------------------------------------------------------------
// clang-format off
enum Square : int {
    A1, B1, C1, D1, E1, F1, G1, H1,
    A2, B2, C2, D2, E2, F2, G2, H2,
    A3, B3, C3, D3, E3, F3, G3, H3,
    A4, B4, C4, D4, E4, F4, G4, H4,
    A5, B5, C5, D5, E5, F5, G5, H5,
    A6, B6, C6, D6, E6, F6, G6, H6,
    A7, B7, C7, D7, E7, F7, G7, H7,
    A8, B8, C8, D8, E8, F8, G8, H8,
    SQ_NB = 64, SQ_NONE = 64
};
// clang-format on

constexpr int file_of(Square s) { return s & 7; }
constexpr int rank_of(Square s) { return s >> 3; }
constexpr Square make_square(int file, int rank) {
    return static_cast<Square>(rank * 8 + file);
}
constexpr bool is_ok(Square s) { return s >= A1 && s <= H8; }

// "e2", "a8", etc. (assumes a valid square)
inline std::string square_to_string(Square s) {
    return std::string{static_cast<char>('a' + file_of(s)),
                       static_cast<char>('1' + rank_of(s))};
}

// ---- Directions (as signed square deltas in LERF) ---------------------------
enum Direction : int {
    NORTH = 8, SOUTH = -8, EAST = 1, WEST = -1,
    NORTH_EAST = 9, NORTH_WEST = 7, SOUTH_EAST = -7, SOUTH_WEST = -9
};

// ---- Castling rights (bitmask) ----------------------------------------------
enum CastlingRight : int {
    NO_CASTLING = 0,
    WHITE_OO = 1, WHITE_OOO = 2,
    BLACK_OO = 4, BLACK_OOO = 8,
    ANY_CASTLING = WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO
};

// ---- Moves ------------------------------------------------------------------
// Packed into 16 bits (Stockfish-style):
//   bits  0-5 : from square
//   bits  6-11: to square
//   bits 12-13: promotion piece (KNIGHT=0, BISHOP=1, ROOK=2, QUEEN=3)
//   bits 14-15: move type flag
enum MoveType : int {
    MT_NORMAL = 0,
    MT_PROMOTION = 1 << 14,
    MT_EN_PASSANT = 2 << 14,
    MT_CASTLING = 3 << 14
};

class Move {
   public:
    Move() = default;
    constexpr explicit Move(std::uint16_t d) : data_(d) {}

    static constexpr Move make(Square from, Square to,
                               MoveType type = MT_NORMAL,
                               PieceType promo = KNIGHT) {
        return Move(static_cast<std::uint16_t>(
            type | ((promo - KNIGHT) << 12) | (to << 6) | from));
    }

    constexpr Square from() const { return static_cast<Square>(data_ & 0x3F); }
    constexpr Square to() const { return static_cast<Square>((data_ >> 6) & 0x3F); }
    constexpr MoveType type() const {
        return static_cast<MoveType>(data_ & (3 << 14));
    }
    constexpr PieceType promotion() const {
        return static_cast<PieceType>(((data_ >> 12) & 3) + KNIGHT);
    }

    constexpr bool is_null() const { return data_ == 0; }
    constexpr std::uint16_t raw() const { return data_; }
    constexpr bool operator==(const Move& o) const { return data_ == o.data_; }
    constexpr bool operator!=(const Move& o) const { return data_ != o.data_; }

    // UCI representation, e.g. "e2e4", "e7e8q".
    std::string to_uci() const {
        if (is_null()) return "0000";
        std::string s = square_to_string(from()) + square_to_string(to());
        if (type() == MT_PROMOTION) {
            constexpr char kPromo[] = {'n', 'b', 'r', 'q'};
            s += kPromo[promotion() - KNIGHT];
        }
        return s;
    }

   private:
    std::uint16_t data_ = 0;
};

constexpr Move kNullMove = Move(0);

}  // namespace shark
