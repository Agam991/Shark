#include "shark/bitboard.hpp"
#include "shark/eval.hpp"
#include "shark/test.hpp"
#include "shark/zobrist.hpp"

// Single entry point that runs every TEST() registered across the test TU's.
int main() {
    shark::init_attacks();  // required before any move generation
    shark::zobrist::init();
    shark::init_eval();
    return shark::test::run_all();
}
