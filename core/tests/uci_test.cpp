#include "shark/test.hpp"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "shark/uci.hpp"

using namespace shark;

namespace {

// Feed a sequence of commands to a fresh Uci and capture everything it writes
// to std::cout.
std::string run_uci(const std::vector<std::string>& cmds) {
    std::ostringstream captured;
    std::streambuf* old = std::cout.rdbuf(captured.rdbuf());

    Uci uci;
    for (const auto& c : cmds) uci.handle(c);
    uci.wait();  // let any async "go" search finish before capturing output

    std::cout.rdbuf(old);
    return captured.str();
}

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

}  // namespace

TEST("uci: handshake reports id + uciok") {
    std::string out = run_uci({"uci"});
    CHECK(contains(out, "id name Shark"));
    CHECK(contains(out, "id author"));
    CHECK(contains(out, "uciok"));
    CHECK(contains(out, "option name Hash"));
    CHECK(contains(out, "option name Threads"));
}

TEST("uci: isready -> readyok") {
    CHECK(contains(run_uci({"isready"}), "readyok"));
}

TEST("uci: position startpos + moves updates the board") {
    // After 1.e4 the FEN should show a white pawn on e4 and black to move.
    std::string out =
        run_uci({"position startpos moves e2e4", "d"});
    CHECK(contains(out, " b "));                         // black to move
    CHECK(contains(out, "rnbqkbnr/pppppppp/8/8/4P3"));   // e4 pushed
}

TEST("uci: position fen is parsed") {
    std::string out = run_uci(
        {"position fen 6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1", "d"});
    CHECK(contains(out, "6k1/5ppp"));
}

TEST("uci: go returns a bestmove") {
    std::string out = run_uci({"position startpos", "go depth 4"});
    CHECK(contains(out, "bestmove "));
    CHECK(contains(out, "info depth"));
}

TEST("uci: go finds mate-in-1 as bestmove") {
    std::string out = run_uci(
        {"position fen 6k1/5ppp/8/8/8/8/5PPP/4R1K1 w - - 0 1", "go depth 3"});
    CHECK(contains(out, "bestmove e1e8"));
    CHECK(contains(out, "score mate 1"));
}

TEST("uci: setoption Hash does not crash and search still works") {
    std::string out = run_uci(
        {"setoption name Hash value 8", "position startpos", "go depth 3"});
    CHECK(contains(out, "bestmove "));
}
