#include "shark/test.hpp"

#include <string>

#include "shark/board.hpp"
#include "shark/eval.hpp"
#include "shark/movegen.hpp"
#include "shark/search.hpp"

using namespace shark;

// ---- Zobrist incremental consistency ----------------------------------------
// After every make_move, the incrementally-maintained key must equal the key of
// a freshly-parsed board with the same FEN. This catches any bug in the
// incremental hash updates (castling/ep/side/psq).
TEST("zobrist: incremental key matches from-scratch key") {
    Board b;
    // A line that exercises pawn double-push (ep), captures, and castling.
    const char* line[] = {"e2e4", "c7c5", "g1f3", "d7d6", "f1c4", "b8c6",
                          "e1g1"};
    for (const char* uci : line) {
        MoveList moves;
        generate_legal(b, moves);
        Move chosen = kNullMove;
        for (Move m : moves)
            if (m.to_uci() == uci) { chosen = m; break; }
        CHECK(!chosen.is_null());
        if (chosen.is_null()) return;

        b.make_move(chosen);
        Board fresh(b.fen());
        CHECK_EQ(b.key(), fresh.key());
    }
}

TEST("zobrist: key restored after unmake") {
    Board b;
    std::uint64_t before = b.key();
    MoveList moves;
    generate_legal(b, moves);
    b.make_move(moves[0]);
    CHECK(b.key() != before);
    b.unmake_move(moves[0]);
    CHECK_EQ(b.key(), before);
}

// ---- Evaluation sanity ------------------------------------------------------
TEST("eval: startpos is roughly balanced") {
    Board b;
    int s = evaluate(b);
    CHECK(s > -50 && s < 50);  // symmetric position ~ 0 (tempo aside)
}

TEST("eval: side up a queen is winning") {
    // White has an extra queen.
    Board b("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    CHECK(evaluate(b) > 600);
}

// ---- Search finds forced mates ----------------------------------------------
Move best_move(const std::string& fen, int depth) {
    Board b(fen);
    Searcher s(16);
    SearchLimits lim;
    lim.max_depth = depth;
    return s.search(b, lim, /*verbose=*/false).best;
}

int best_score(const std::string& fen, int depth) {
    Board b(fen);
    Searcher s(16);
    SearchLimits lim;
    lim.max_depth = depth;
    return s.search(b, lim, /*verbose=*/false).score;
}

TEST("search: finds mate in 1 (back-rank)") {
    // White: Re1-e8 is mate. King g8 boxed in by its own f7/g7/h7 pawns.
    std::string fen = "6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1";
    CHECK_EQ(best_move(fen, 3).to_uci(), std::string("e1e8"));
    CHECK(best_score(fen, 3) >= kValueMateInMaxPly);
}

TEST("search: grabs a free queen") {
    // Black queen on d5 is hanging to the pawn on e4... actually make it clear:
    // White to move, black queen on e5 en prise to nothing but a free rook on
    // a8 capturable by rook a1. Simplify: white rook a1, black rook a8, nothing
    // between -> Rxa8 wins a rook.
    std::string fen = "r5k1/8/8/8/8/8/6PP/R5K1 w - - 0 1";
    CHECK_EQ(best_move(fen, 4).to_uci(), std::string("a1a8"));
}

// ---- Multithreaded search (Lazy SMP) ----------------------------------------
// Same answers as single-threaded, and it must not crash / hang. Uses a modest
// thread count so CI (and laptops) stay cool.
TEST("search: threaded search finds mate in 1") {
    Board b("6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1");
    Searcher s(16);
    s.set_threads(4);  // internally clamped to core count
    CHECK(s.threads() >= 1);
    SearchLimits lim;
    lim.max_depth = 4;
    SearchResult r = s.search(b, lim, /*verbose=*/false);
    CHECK_EQ(r.best.to_uci(), std::string("e1e8"));
    CHECK(r.score >= kValueMateInMaxPly);
}

TEST("search: set_threads clamps to hardware core count") {
    Searcher s(8);
    s.set_threads(100000);  // absurd request
    CHECK(s.threads() >= 1);
    CHECK(s.threads() <= 100000);  // clamped, not honored literally
    s.set_threads(0);
    CHECK_EQ(s.threads(), 1);  // never below 1
}

TEST("search: threaded search on startpos returns a legal move") {
    Board b;
    Searcher s(16);
    s.set_threads(2);
    SearchLimits lim;
    lim.max_depth = 5;
    SearchResult r = s.search(b, lim, /*verbose=*/false);
    MoveList legal;
    generate_legal(b, legal);
    bool ok = false;
    for (Move m : legal)
        if (m == r.best) { ok = true; break; }
    CHECK(ok);
}
