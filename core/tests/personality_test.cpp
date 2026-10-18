#include "shark/test.hpp"

#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "shark/eval.hpp"
#include "shark/movegen.hpp"
#include "shark/personality.hpp"
#include "shark/uci.hpp"

using namespace shark;

namespace {
const Personality kAll[] = {Personality::Balanced, Personality::Aggressive,
                            Personality::Defensive, Personality::Positional};

bool is_legal(const Board& b, Move m) {
    Board c = b;
    MoveList l;
    generate_legal(c, l);
    for (Move x : l) if (x == m) return true;
    return false;
}
}  // namespace

TEST("personality: names round-trip and unknown is rejected") {
    for (Personality p : kAll) {
        Personality out = Personality::Balanced;
        CHECK(parse_personality(personality_name(p), out));
        CHECK(out == p);
    }
    Personality x = Personality::Defensive;
    CHECK(!parse_personality("reckless", x));
    CHECK(x == Personality::Defensive);
    CHECK(parse_personality("aGGressive", x));
    CHECK(x == Personality::Aggressive);
}

TEST("personality: every repertoire line is legal") {
    CHECK_EQ(personality_book_bad_lines(), 0);
}

TEST("personality: book gives legal moves for non-Balanced only") {
    std::mt19937 rng(1);
    Board start;
    CHECK(personality_book_move(start, Personality::Balanced, rng).is_null());
    for (Personality p : {Personality::Aggressive, Personality::Positional}) {
        Move m = personality_book_move(start, p, rng);
        CHECK(!m.is_null());
        CHECK(is_legal(start, m));
    }
    // Defensive has no White line starting 1.e4: it opens with 1.d4.
    Move d = personality_book_move(start, Personality::Defensive, rng);
    CHECK(d.to_uci() == "d2d4");
}

TEST("personality: book answers 1.e4 differently per personality") {
    std::mt19937 rng(7);
    Board b;
    MoveList l;
    generate_legal(b, l);
    for (Move m : l) if (m.to_uci() == "e2e4") b.make_move(m);
    Move agg = personality_book_move(b, Personality::Aggressive, rng);
    Move def = personality_book_move(b, Personality::Defensive, rng);
    CHECK(agg.to_uci() == "c7c5");
    CHECK(def.to_uci() == "c7c6" || def.to_uci() == "e7e6");
}

TEST("personality: evaluation is symmetric and zero at the start") {
    Board start;
    Board a("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");
    Board f("rnbqk2r/pppp1ppp/5n2/2b1p3/4P3/2N2N2/PPPP1PPP/R1BQKB1R b KQkq - 4 4");
    for (Personality p : kAll) {
        set_personality(p);
        CHECK_EQ(evaluate(start), 0);
        CHECK_EQ(evaluate(a), evaluate(f));
    }
    set_personality(Personality::Balanced);
}

TEST("personality: styles actually change the evaluation") {
    Board b("r1bq1rk1/pp2bppp/2n1pn2/2pp4/3P1B2/2PBPN2/PP1N1PPP/R2QK2R w KQ - 0 8");
    set_personality(Personality::Balanced);
    int bal = evaluate(b);
    int agg = (set_personality(Personality::Aggressive), evaluate(b));
    int def = (set_personality(Personality::Defensive), evaluate(b));
    int pos = (set_personality(Personality::Positional), evaluate(b));
    set_personality(Personality::Balanced);
    CHECK(agg != bal);
    CHECK(def != bal);
    CHECK(pos != bal);
    CHECK(agg != def || def != pos);
}

TEST("personality: UCI option, book move and search still work") {
    std::ostringstream captured;
    std::streambuf* old = std::cout.rdbuf(captured.rdbuf());
    {
        Uci uci;
        uci.handle("uci");
        uci.handle("setoption name Personality value Aggressive");
        uci.handle("position startpos");
        uci.handle("go depth 3");  // book: 1.e4
        uci.wait();
        uci.handle("position startpos moves e2e4 e7e5 g1f3 b8c6 a2a3");
        uci.handle("go depth 3");  // out of book: real search
        uci.wait();
        uci.handle("setoption name PersonalityBook value false");
        uci.handle("position startpos");
        uci.handle("go depth 3");
        uci.wait();
    }
    std::cout.rdbuf(old);
    std::string out = captured.str();
    CHECK(out.find("option name Personality type combo") != std::string::npos);
    CHECK(out.find("Aggressive book move") != std::string::npos);
    CHECK(out.find("info depth") != std::string::npos);
    CHECK(out.find("bestmove e2e4") != std::string::npos);
    set_personality(Personality::Balanced);
}
