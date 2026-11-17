#include "shark/test.hpp"

#include <cstdlib>
#include <string>

#include "shark/board.hpp"
#include "shark/movegen.hpp"
#include "shark/search.hpp"

using namespace shark;

namespace {
// Play a move given in UCI text (e.g. "g1f3"); returns false if not legal.
bool play(Board& b, const std::string& uci) {
    MoveList legal;
    generate_legal(b, legal);
    for (Move m : legal) {
        if (m.to_uci() == uci) {
            b.make_move(m);
            return true;
        }
    }
    return false;
}
}  // namespace

TEST("repetition: threefold is detected only on the third occurrence") {
    Board b;  // start position = occurrence #1
    CHECK(!b.is_repetition(0));

    for (const char* mv : {"g1f3", "g8f6", "f3g1", "f6g8"}) CHECK(play(b, mv));
    CHECK(!b.is_repetition(0));  // occurrence #2: not yet a draw on game history

    for (const char* mv : {"g1f3", "g8f6", "f3g1", "f6g8"}) CHECK(play(b, mv));
    CHECK(b.is_repetition(0));  // occurrence #3: threefold
}

TEST("repetition: one repeat inside the search tree already counts") {
    Board b;
    for (const char* mv : {"g1f3", "g8f6", "f3g1", "f6g8"}) CHECK(play(b, mv));
    // Pretend these 4 plies were made inside the search (ply = 4 + 1):
    // the start position reappearing 4 plies ago is a twofold within the tree.
    CHECK(b.is_repetition(5));
}

TEST("repetition: a pawn move makes earlier positions unreachable") {
    Board b;
    for (const char* mv : {"g1f3", "g8f6", "f3g1", "f6g8", "e2e4", "e7e5"})
        CHECK(play(b, mv));
    for (const char* mv : {"g1f3", "g8f6", "f3g1", "f6g8"}) CHECK(play(b, mv));
    // Position after e4 e5 now occurred twice, but only twice -> not threefold.
    CHECK(!b.is_repetition(0));
}

TEST("fifty-move: search treats a 100-halfmove position as a draw") {
    // King + rook vs king is a clear win, but the 50-move rule makes it a draw.
    Board drawn("4k3/8/8/8/8/8/8/4K2R w K - 100 80");
    Board fresh("4k3/8/8/8/8/8/8/4K2R w K - 0 80");
    CHECK(drawn.halfmove_clock() == 100);

    SearchLimits lim;
    lim.max_depth = 4;
    Searcher s(16);
    SearchResult rd = s.search(drawn, lim, false);
    SearchResult rf = s.search(fresh, lim, false);

    CHECK(std::abs(rd.score) <= 20);  // ~0 (draw, tiny contempt)
    CHECK(rf.score > 200);            // normal winning score
    CHECK(!rd.best.is_null());        // root still returns a move
}