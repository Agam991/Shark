#include "shark/personality.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "shark/movegen.hpp"

namespace shark {

namespace {

std::atomic<int> g_personality{static_cast<int>(Personality::Balanced)};

// Order matches the enum: Balanced, Aggressive, Defensive, Positional.
constexpr EvalWeights kWeights[] = {
    //  mob kAtt kPrx shld kExp dbl iso pass%  rkO rkS bPair outp ctr space
    {0, 0, 0, 0, 0, 0, 0, 100, 0, 0, 0, 0, 0, 0},       // Balanced (unused)
    {2, 3, 3, 0, 0, 5, 5, 110, 15, 8, 20, 10, 2, 4},    // Aggressive
    {1, 1, 0, 12, 3, 15, 15, 80, 8, 4, 15, 8, 1, 0},    // Defensive
    {3, 1, 0, 6, 1, 18, 14, 130, 20, 10, 35, 22, 4, 1}, // Positional
};

struct Line {
    Personality who;
    Color side;        // which side the personality plays in this line
    const char* moves; // UCI moves from the start position
};

// Only the moves made by `side` are stored as book moves; the other side's
// moves just walk the position forward.
constexpr Line kLines[] = {
    // ---------------- Aggressive ----------------
    {Personality::Aggressive, WHITE, "e2e4 e7e5 f2f4 e5f4 g1f3 g7g5 h2h4 g5g4 f3e5"},                    // King's Gambit
    {Personality::Aggressive, WHITE, "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5 b2b4 c5b4 c2c3 b4a5 d2d4"},          // Evans Gambit
    {Personality::Aggressive, WHITE, "e2e4 c7c5 b1c3 b8c6 f2f4 g7g6 g1f3 f8g7 f1c4 e7e6"},               // Grand Prix Attack
    {Personality::Aggressive, WHITE, "e2e4 e7e6 d2d4 d7d5 b1c3 f8b4 e4e5 c7c5 a2a3 b4c3 b2c3 g8e7 d1g4"}, // Winawer, Qg4
    {Personality::Aggressive, WHITE, "e2e4 c7c6 d2d4 d7d5 e4d5 c6d5 c2c4 g8f6 b1c3 e7e6 g1f3 f8e7 c4d5 f6d5"}, // Panov
    {Personality::Aggressive, BLACK, "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3 a7a6 c1e3 e7e5 d4b3 c8e6"}, // Najdorf
    {Personality::Aggressive, BLACK, "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 g1f3 e8g8 f1e2 e7e5"},     // King's Indian
    {Personality::Aggressive, BLACK, "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6 g2g3 f8b4"},                         // Reversed Sicilian
    {Personality::Aggressive, BLACK, "g1f3 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6 d2d4 e8g8"},              // KID by transposition

    // ---------------- Defensive ----------------
    {Personality::Defensive, WHITE, "d2d4 d7d5 c1f4 g8f6 e2e3 e7e6 g1f3 c7c5 c2c3 b8c6 b1d2 f8d6 f4g3"}, // London System
    {Personality::Defensive, WHITE, "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 e8g8 g1f3 b8d7 d1c2"}, // QGD
    {Personality::Defensive, BLACK, "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4 c8f5 e4g3 f5g6 h2h4 h7h6 g1f3 b8d7"}, // Caro-Kann
    {Personality::Defensive, BLACK, "e2e4 e7e6 d2d4 d7d5 b1c3 g8f6 c1g5 f8e7 e4e5 f6d7 g5e7 d8e7 f2f4 a7a6"}, // French Classical
    {Personality::Defensive, BLACK, "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6 c1g5 f8e7 e2e3 e8g8 g1f3 b8d7"},     // QGD
    {Personality::Defensive, BLACK, "c2c4 e7e6 b1c3 d7d5 d2d4 g8f6 c1g5 f8e7"},                         // QGD via English
    {Personality::Defensive, BLACK, "g1f3 d7d5 g2g3 g8f6 f1g2 e7e6 e1g1 f8e7 d2d3 e8g8"},              // Solid vs Reti

    // ---------------- Positional ----------------
    {Personality::Positional, WHITE, "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6 g2g3 d7d5 c4d5 f6d5 f1g2 d5b6 e1g1 f8e7"}, // English
    {Personality::Positional, WHITE, "c2c4 c7c5 g1f3 g8f6 b1c3 e7e6 g2g3 b7b6 f1g2 c8b7 e1g1 f8e7"},     // Symmetrical English
    {Personality::Positional, WHITE, "d2d4 g8f6 c2c4 e7e6 g2g3 d7d5 f1g2 f8e7 g1f3 e8g8 e1g1 d5c4 d1c2 a7a6"}, // Catalan
    {Personality::Positional, WHITE, "g1f3 d7d5 c2c4 e7e6 g2g3 g8f6 f1g2 f8e7 e1g1 e8g8 b2b3 c7c5"},    // Reti
    {Personality::Positional, BLACK, "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6 b5a4 g8f6 e1g1 f8e7 f1e1 b7b5 a4b3 d7d6 c2c3 e8g8 h2h3 c6a5 b3c2 c7c5 d2d4 d8c7"}, // Closed Ruy Lopez
    {Personality::Positional, BLACK, "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4 e2e3 e8g8 f1d3 d7d5 g1f3 c7c5 e1g1 b8c6"}, // Nimzo-Indian
    {Personality::Positional, BLACK, "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6 g2g3 d7d5 c4d5 f6d5 f1g2 d5b6 e1g1 f8e7"}, // Reversed Sicilian
    {Personality::Positional, BLACK, "g1f3 d7d5 g2g3 g8f6 f1g2 e7e6 e1g1 f8e7 d2d3 e8g8"},              // Reti setup
};

using BookMap = std::unordered_map<std::uint64_t, std::vector<Move>>;
BookMap g_book[static_cast<int>(Personality::Count)];
int g_bad_lines = 0;
std::once_flag g_book_once;

void build_book() {
    for (const Line& line : kLines) {
        BookMap& book = g_book[static_cast<int>(line.who)];
        Board b;  // start position
        std::istringstream ss(line.moves);
        std::string tok;
        bool ok = true;
        while (ss >> tok) {
            MoveList legal;
            generate_legal(b, legal);
            Move found = kNullMove;
            for (Move m : legal)
                if (m.to_uci() == tok) { found = m; break; }
            if (found.is_null()) { ok = false; break; }
            if (b.side_to_move() == line.side) {
                auto& v = book[b.key()];
                if (std::find(v.begin(), v.end(), found) == v.end())
                    v.push_back(found);
            }
            b.make_move(found);
        }
        if (!ok) ++g_bad_lines;
    }
}

}  // namespace

const char* personality_name(Personality p) {
    switch (p) {
        case Personality::Aggressive: return "Aggressive";
        case Personality::Defensive:  return "Defensive";
        case Personality::Positional: return "Positional";
        default:                      return "Balanced";
    }
}

bool parse_personality(const std::string& name, Personality& out) {
    std::string n;
    for (char c : name) n += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (int i = 0; i < static_cast<int>(Personality::Count); ++i) {
        std::string cand = personality_name(static_cast<Personality>(i));
        for (char& c : cand) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (cand == n) { out = static_cast<Personality>(i); return true; }
    }
    return false;
}

Personality current_personality() {
    return static_cast<Personality>(g_personality.load(std::memory_order_relaxed));
}
void set_personality(Personality p) {
    g_personality.store(static_cast<int>(p), std::memory_order_relaxed);
}

const EvalWeights& personality_weights(Personality p) {
    return kWeights[static_cast<int>(p)];
}

Move personality_book_move(const Board& b, Personality p, std::mt19937& rng) {
    if (p == Personality::Balanced) return kNullMove;
    std::call_once(g_book_once, build_book);
    const BookMap& book = g_book[static_cast<int>(p)];
    auto it = book.find(b.key());
    if (it == book.end() || it->second.empty()) return kNullMove;

    // Defensive copy: only return a move that is legal in this exact position.
    Board copy = b;
    MoveList legal;
    generate_legal(copy, legal);
    std::vector<Move> ok;
    for (Move m : it->second)
        for (Move l : legal)
            if (l == m) { ok.push_back(m); break; }
    if (ok.empty()) return kNullMove;
    return ok[std::uniform_int_distribution<std::size_t>(0, ok.size() - 1)(rng)];
}

int personality_book_bad_lines() {
    std::call_once(g_book_once, build_book);
    return g_bad_lines;
}

}  // namespace shark
