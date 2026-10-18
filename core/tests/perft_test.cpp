#include "shark/test.hpp"

#include <array>
#include <cstdint>
#include <string_view>

#include "shark/bitboard.hpp"
#include "shark/board.hpp"
#include "shark/perft.hpp"

// ============================================================================
// Perft (performance test) reference data + live checks.
//
// Perft counts the number of leaf nodes at a given depth from a position. It is
// THE correctness gate for move generation. These numbers are the well-known
// published values from the Chess Programming Wiki.
// ============================================================================

using shark::Board;
using shark::perft;

namespace {

struct PerftEntry {
    int depth;
    std::uint64_t nodes;
};

struct PerftPosition {
    std::string_view name;
    std::string_view fen;
    std::array<PerftEntry, 6> expected;  // (depth, nodes); depth 0 == unused
};

// Startpos
constexpr PerftPosition kStartPos{
    "startpos",
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    {{{1, 20}, {2, 400}, {3, 8902}, {4, 197281}, {5, 4865609}, {6, 0}}}};

// Kiwipete - dense tactical position, catches most movegen bugs.
constexpr PerftPosition kKiwipete{
    "kiwipete",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    {{{1, 48}, {2, 2039}, {3, 97862}, {4, 4085603}, {5, 0}, {6, 0}}}};

// Position 3 - endgame, tricky pawn/ep interactions.
constexpr PerftPosition kPos3{
    "position3",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    {{{1, 14}, {2, 191}, {3, 2812}, {4, 43238}, {5, 674624}, {6, 0}}}};

// Position 4 - many promotions/castling edge cases.
constexpr PerftPosition kPos4{
    "position4",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    {{{1, 6}, {2, 264}, {3, 9467}, {4, 422333}, {5, 0}, {6, 0}}}};

// Position 5 - Talkchess perft, notorious bug-catcher.
constexpr PerftPosition kPos5{
    "position5",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    {{{1, 44}, {2, 1486}, {3, 62379}, {4, 2103487}, {5, 0}, {6, 0}}}};

void check_position(const PerftPosition& pos, int& sf_failures) {
    Board b(std::string(pos.fen));
    for (auto [depth, nodes] : pos.expected) {
        if (nodes == 0) continue;  // un-tabulated depth
        std::uint64_t got = perft(b, depth);
        if (got != nodes) {
            std::printf("    PERFT MISMATCH [%s] depth %d: got %llu, want %llu\n",
                        std::string(pos.name).c_str(), depth,
                        (unsigned long long)got, (unsigned long long)nodes);
            ++sf_failures;
        }
    }
}

}  // namespace

TEST("perft: startpos") { check_position(kStartPos, shk_failures); }
TEST("perft: kiwipete") { check_position(kKiwipete, shk_failures); }
TEST("perft: position 3") { check_position(kPos3, shk_failures); }
TEST("perft: position 4") { check_position(kPos4, shk_failures); }
TEST("perft: position 5") { check_position(kPos5, shk_failures); }
