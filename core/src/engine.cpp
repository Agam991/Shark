#include "shark/bitboard.hpp"
#include "shark/eval.hpp"
#include "shark/uci.hpp"
#include "shark/zobrist.hpp"

// Shark engine entry point: a UCI-speaking chess engine.
//
// Initializes the static tables, then hands control to the UCI loop. Any UCI
// GUI or tournament tool (cutechess-cli, Cute Chess, Arena) can now drive it,
// including matches against Stockfish.

int main() {
    shark::init_attacks();
    shark::zobrist::init();
    shark::init_eval();

    shark::Uci uci;
    uci.loop();
    return 0;
}
