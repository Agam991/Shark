#include "shark/board.hpp"

#include <cctype>
#include <sstream>

#include "shark/zobrist.hpp"

namespace shark {

namespace {

// AND-mask of castling rights to KEEP when the piece on/into a square moves.
// Any square not listed keeps all rights (~0). We clear the relevant bits for
// the king-start and rook-start squares.
struct CastlingMasks {
    int mask[SQ_NB];
    CastlingMasks() {
        for (int i = 0; i < SQ_NB; ++i) mask[i] = ANY_CASTLING;
        mask[E1] = ~(WHITE_OO | WHITE_OOO) & ANY_CASTLING;
        mask[E8] = ~(BLACK_OO | BLACK_OOO) & ANY_CASTLING;
        mask[A1] = ~WHITE_OOO & ANY_CASTLING;
        mask[H1] = ~WHITE_OO & ANY_CASTLING;
        mask[A8] = ~BLACK_OOO & ANY_CASTLING;
        mask[H8] = ~BLACK_OO & ANY_CASTLING;
    }
};
const CastlingMasks kCastling;

PieceType char_to_pt(char c) {
    switch (std::tolower(c)) {
        case 'p': return PAWN;
        case 'n': return KNIGHT;
        case 'b': return BISHOP;
        case 'r': return ROOK;
        case 'q': return QUEEN;
        case 'k': return KING;
        default: return NO_PIECE_TYPE;
    }
}

char pt_to_char(Color c, PieceType pt) {
    const char* letters = "pnbrqk";
    char ch = letters[pt];
    return c == WHITE ? static_cast<char>(std::toupper(ch)) : ch;
}

}  // namespace

// ---- Low-level board mutation ----------------------------------------------
void Board::put_piece(Color c, PieceType pt, Square s) {
    Bitboard b = square_bb(s);
    bb_[c][pt] |= b;
    occ_[c] |= b;
    mailbox_[s] = Piece{c, pt};
    key_ ^= zobrist::psq[c][pt][s];
}

void Board::remove_piece(Square s) {
    Piece p = mailbox_[s];
    Bitboard b = square_bb(s);
    bb_[p.color][p.type] &= ~b;
    occ_[p.color] &= ~b;
    mailbox_[s] = Piece{};
    key_ ^= zobrist::psq[p.color][p.type][s];
}

void Board::move_piece(Square from, Square to) {
    Piece p = mailbox_[from];
    Bitboard fromto = square_bb(from) | square_bb(to);
    bb_[p.color][p.type] ^= fromto;
    occ_[p.color] ^= fromto;
    mailbox_[from] = Piece{};
    mailbox_[to] = p;
    key_ ^= zobrist::psq[p.color][p.type][from] ^
            zobrist::psq[p.color][p.type][to];
}

// ---- FEN --------------------------------------------------------------------
void Board::set_startpos() {
    set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

bool Board::set_fen(const std::string& fen) {
    bb_ = {};
    occ_ = {};
    for (auto& sq : mailbox_) sq = Piece{};
    castling_ = NO_CASTLING;
    ep_ = SQ_NONE;
    halfmove_clock_ = 0;
    fullmove_number_ = 1;
    key_ = 0;
    history_.clear();

    std::istringstream ss(fen);
    std::string placement, active, castle, ep;
    ss >> placement >> active >> castle >> ep;
    int hm = 0, fm = 1;
    ss >> hm >> fm;

    int file = 0, rank = 7;
    for (char c : placement) {
        if (c == '/') {
            --rank;
            file = 0;
        } else if (std::isdigit(c)) {
            file += c - '0';
        } else {
            PieceType pt = char_to_pt(c);
            if (pt == NO_PIECE_TYPE) return false;
            Color col = std::isupper(c) ? WHITE : BLACK;
            put_piece(col, pt, make_square(file, rank));
            ++file;
        }
    }

    stm_ = (active == "b") ? BLACK : WHITE;

    for (char c : castle) {
        switch (c) {
            case 'K': castling_ |= WHITE_OO; break;
            case 'Q': castling_ |= WHITE_OOO; break;
            case 'k': castling_ |= BLACK_OO; break;
            case 'q': castling_ |= BLACK_OOO; break;
            default: break;
        }
    }

    if (ep != "-" && ep.size() == 2) {
        ep_ = make_square(ep[0] - 'a', ep[1] - '1');
    }
    halfmove_clock_ = hm;
    fullmove_number_ = fm;

    // Finalize the zobrist key: piece placement was accumulated by put_piece;
    // now fold in castling, en passant, and side to move.
    key_ ^= zobrist::castling[castling_ & ANY_CASTLING];
    if (ep_ != SQ_NONE) key_ ^= zobrist::ep_file[file_of(ep_)];
    if (stm_ == BLACK) key_ ^= zobrist::side;
    return true;
}

std::string Board::fen() const {
    std::ostringstream ss;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            Piece p = mailbox_[make_square(file, rank)];
            if (p.is_none()) {
                ++empty;
            } else {
                if (empty) { ss << empty; empty = 0; }
                ss << pt_to_char(p.color, p.type);
            }
        }
        if (empty) ss << empty;
        if (rank) ss << '/';
    }
    ss << ' ' << (stm_ == WHITE ? 'w' : 'b') << ' ';

    std::string castle;
    if (castling_ & WHITE_OO) castle += 'K';
    if (castling_ & WHITE_OOO) castle += 'Q';
    if (castling_ & BLACK_OO) castle += 'k';
    if (castling_ & BLACK_OOO) castle += 'q';
    ss << (castle.empty() ? "-" : castle) << ' ';

    ss << (ep_ == SQ_NONE ? "-" : square_to_string(ep_)) << ' ';
    ss << halfmove_clock_ << ' ' << fullmove_number_;
    return ss.str();
}

// ---- Attack detection -------------------------------------------------------
bool Board::is_square_attacked(Square s, Color by) const {
    Bitboard occ = pieces();

    if (pawn_attacks(~by, s) & bb_[by][PAWN]) return true;
    if (knight_attacks(s) & bb_[by][KNIGHT]) return true;
    if (king_attacks(s) & bb_[by][KING]) return true;

    Bitboard bishops_queens = bb_[by][BISHOP] | bb_[by][QUEEN];
    if (bishop_attacks(s, occ) & bishops_queens) return true;

    Bitboard rooks_queens = bb_[by][ROOK] | bb_[by][QUEEN];
    if (rook_attacks(s, occ) & rooks_queens) return true;

    return false;
}

// ---- Make / unmake ----------------------------------------------------------
void Board::make_move(Move m) {
    Square from = m.from();
    Square to = m.to();
    MoveType mt = m.type();
    Piece moving = mailbox_[from];
    Color us = stm_;
    Color them = ~us;

    StateInfo st{castling_, ep_, halfmove_clock_, Piece{}, key_};

    // Fold OUT the old en-passant and castling contributions; we fold the new
    // ones back in after the mutations. (side flips unconditionally below.)
    if (ep_ != SQ_NONE) key_ ^= zobrist::ep_file[file_of(ep_)];
    key_ ^= zobrist::castling[castling_ & ANY_CASTLING];

    // Determine captured piece (en passant is special: victim is behind `to`).
    Square capture_sq = to;
    if (mt == MT_EN_PASSANT) {
        capture_sq = make_square(file_of(to), rank_of(from));
    }
    if (!mailbox_[capture_sq].is_none() && mt != MT_CASTLING) {
        st.captured = mailbox_[capture_sq];
        remove_piece(capture_sq);
    }

    history_.push_back(st);

    // Move the piece(s).
    if (mt == MT_CASTLING) {
        move_piece(from, to);  // king
        Square rook_from, rook_to;
        switch (to) {
            case G1: rook_from = H1; rook_to = F1; break;
            case C1: rook_from = A1; rook_to = D1; break;
            case G8: rook_from = H8; rook_to = F8; break;
            default: rook_from = A8; rook_to = D8; break;  // C8
        }
        move_piece(rook_from, rook_to);
    } else if (mt == MT_PROMOTION) {
        remove_piece(from);
        put_piece(us, m.promotion(), to);
    } else {
        move_piece(from, to);
    }

    // En passant target: only after a double pawn push.
    ep_ = SQ_NONE;
    if (moving.type == PAWN && std::abs(to - from) == 16) {
        ep_ = static_cast<Square>((from + to) / 2);
    }

    // Castling rights: clear bits touched by from/to squares.
    castling_ &= kCastling.mask[from];
    castling_ &= kCastling.mask[to];

    // Fold the NEW en-passant / castling contributions back into the key.
    if (ep_ != SQ_NONE) key_ ^= zobrist::ep_file[file_of(ep_)];
    key_ ^= zobrist::castling[castling_ & ANY_CASTLING];
    key_ ^= zobrist::side;  // side to move flips

    // Halfmove clock: reset on pawn move or capture.
    if (moving.type == PAWN || !st.captured.is_none()) {
        halfmove_clock_ = 0;
    } else {
        ++halfmove_clock_;
    }
    if (us == BLACK) ++fullmove_number_;

    stm_ = them;
}

void Board::unmake_move(Move m) {
    stm_ = ~stm_;  // back to the mover
    Color us = stm_;

    Square from = m.from();
    Square to = m.to();
    MoveType mt = m.type();

    StateInfo st = history_.back();
    history_.pop_back();

    // Reverse the piece movement.
    if (mt == MT_CASTLING) {
        move_piece(to, from);  // king back
        Square rook_from, rook_to;
        switch (to) {
            case G1: rook_from = H1; rook_to = F1; break;
            case C1: rook_from = A1; rook_to = D1; break;
            case G8: rook_from = H8; rook_to = F8; break;
            default: rook_from = A8; rook_to = D8; break;  // C8
        }
        move_piece(rook_to, rook_from);  // rook back
    } else if (mt == MT_PROMOTION) {
        remove_piece(to);
        put_piece(us, PAWN, from);
    } else {
        move_piece(to, from);
    }

    // Restore a captured piece.
    if (!st.captured.is_none()) {
        Square capture_sq = to;
        if (mt == MT_EN_PASSANT) {
            capture_sq = make_square(file_of(to), rank_of(from));
        }
        put_piece(st.captured.color, st.captured.type, capture_sq);
    }

    castling_ = st.castling;
    ep_ = st.ep_square;
    halfmove_clock_ = st.halfmove_clock;
    key_ = st.key;
    if (us == BLACK) --fullmove_number_;
}

// ---- Null move (pass the turn) ----------------------------------------------
void Board::make_null_move() {
    StateInfo st{castling_, ep_, halfmove_clock_, Piece{}, key_};
    history_.push_back(st);

    if (ep_ != SQ_NONE) {
        key_ ^= zobrist::ep_file[file_of(ep_)];
        ep_ = SQ_NONE;
    }
    key_ ^= zobrist::side;
    ++halfmove_clock_;
    stm_ = ~stm_;
}

void Board::unmake_null_move() {
    stm_ = ~stm_;
    StateInfo st = history_.back();
    history_.pop_back();
    castling_ = st.castling;
    ep_ = st.ep_square;
    halfmove_clock_ = st.halfmove_clock;
    key_ = st.key;
}

}  // namespace shark
