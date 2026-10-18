#include "shark/uci.hpp"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "shark/eval.hpp"
#include "shark/movegen.hpp"
#include "shark/perft.hpp"
#include "shark/personality.hpp"
#include "shark/version.hpp"

namespace shark {

Uci::Uci() : searcher_(64), rng_(std::random_device{}()) {
    set_personality(Personality::Balanced);  // fresh engine = original style
}  // 64 MB default TT (matches hash_mb_ default)

Uci::~Uci() { stop_search(); }  // never leave a search thread dangling

void Uci::wait() {
    if (search_thread_.joinable()) search_thread_.join();  // no stop request
}

Move Uci::parse_move(const std::string& uci) const {
    Board copy = board_;
    MoveList legal;
    generate_legal(copy, legal);
    for (Move m : legal)
        if (m.to_uci() == uci) return m;
    return kNullMove;
}

void Uci::cmd_uci() {
    std::cout << "id name " << kEngineName << ' ' << kVersionString << "\n";
    std::cout << "id author " << kAuthor << "\n";
    std::cout << "option name Hash type spin default 64 min 1 max 4096\n";
    std::cout << "option name Threads type spin default 1 min 1 max 256\n";
    std::cout << "option name Personality type combo default Balanced"
                 " var Balanced var Aggressive var Defensive var Positional\n";
    std::cout << "option name PersonalityBook type check default true\n";
    std::cout << "uciok\n";
    std::cout.flush();
}

void Uci::cmd_isready() {
    std::cout << "readyok\n";
    std::cout.flush();
}

void Uci::cmd_newgame() {
    stop_search();
    searcher_.new_game();
    board_.set_startpos();
}

void Uci::cmd_setoption(const std::string& args) {
    // Format: name <Name> value <Value>
    std::istringstream ss(args);
    std::string tok, name, value;
    ss >> tok;  // "name"
    // Names can be multi-word; collect until "value".
    while (ss >> tok && tok != "value") {
        if (!name.empty()) name += ' ';
        name += tok;
    }
    ss >> value;

    if (name == "Hash") {
        hash_mb_ = std::max(1, std::stoi(value));
        searcher_.set_tt_size(hash_mb_);
    } else if (name == "Threads") {
        threads_ = std::max(1, std::stoi(value));
        searcher_.set_threads(threads_);  // internally capped to core count
    } else if (name == "Personality") {
        Personality p;
        if (parse_personality(value, p)) {
            stop_search();
            set_personality(p);
            searcher_.new_game();  // TT scores belong to the old evaluation
        }
    } else if (name == "PersonalityBook") {
        use_book_ = (value != "false" && value != "0");
    }
}

void Uci::cmd_position(const std::string& args) {
    std::istringstream ss(args);
    std::string tok;
    ss >> tok;

    if (tok == "startpos") {
        board_.set_startpos();
        ss >> tok;  // maybe "moves"
    } else if (tok == "fen") {
        std::string fen;
        // FEN is 6 space-separated fields.
        for (int i = 0; i < 6 && ss >> tok && tok != "moves"; ++i) {
            if (!fen.empty()) fen += ' ';
            fen += tok;
        }
        board_.set_fen(fen);
        // tok currently holds "moves" or the last FEN field; normalize.
        if (tok != "moves") ss >> tok;
    }

    if (tok == "moves") {
        std::string mv;
        while (ss >> mv) {
            Move m = parse_move(mv);
            if (m.is_null()) break;  // illegal / malformed; stop applying
            board_.make_move(m);
        }
    }
}

void Uci::cmd_go(const std::string& args) {
    // A search may still be running (e.g. "go infinite"); stop and join it first
    // so we never spawn overlapping searches (which would double-book cores).
    stop_search();

    std::istringstream ss(args);
    std::string tok;

    SearchLimits limits;
    limits.max_depth = 64;
    std::int64_t wtime = 0, btime = 0, winc = 0, binc = 0, movetime = 0;
    int movestogo = 0;
    bool infinite = false;

    while (ss >> tok) {
        if (tok == "depth") ss >> limits.max_depth;
        else if (tok == "nodes") ss >> limits.max_nodes;
        else if (tok == "movetime") ss >> movetime;
        else if (tok == "wtime") ss >> wtime;
        else if (tok == "btime") ss >> btime;
        else if (tok == "winc") ss >> winc;
        else if (tok == "binc") ss >> binc;
        else if (tok == "movestogo") ss >> movestogo;
        else if (tok == "infinite") infinite = true;
    }

    // Personality opening repertoire: play instantly while still in book.
    if (!infinite && use_book_) {
        Move bm = personality_book_move(board_, current_personality(), rng_);
        if (!bm.is_null()) {
            std::cout << "info string " << personality_name(current_personality())
                      << " book move\n"
                      << "bestmove " << bm.to_uci() << "\n";
            std::cout.flush();
            return;
        }
    }

    if (movetime > 0) {
        limits.movetime_ms = movetime;
    } else if (!infinite && (wtime > 0 || btime > 0)) {
        // Simple time budget: share of remaining time + increment, with margin.
        std::int64_t my_time = (board_.side_to_move() == WHITE) ? wtime : btime;
        std::int64_t my_inc = (board_.side_to_move() == WHITE) ? winc : binc;
        int moves = movestogo > 0 ? movestogo : 30;
        std::int64_t budget = my_time / moves + my_inc;
        // Never use more than ~half the remaining clock; keep a small margin.
        budget = std::min(budget, my_time / 2);
        limits.movetime_ms = std::max<std::int64_t>(5, budget - 10);
    }
    // else: depth/nodes/infinite -> no time cap (infinite runs to max_depth).

    // Run the search on a background thread so "stop"/"quit" can interrupt it.
    // Copy the board into the searcher's thread via a snapshot.
    Board snapshot = board_;
    search_thread_ = std::thread([this, snapshot, limits]() mutable {
        SearchResult r = searcher_.search(snapshot, limits, /*verbose=*/true);
        std::cout << "bestmove "
                  << (r.best.is_null() ? "0000" : r.best.to_uci()) << "\n";
        std::cout.flush();
    });
}

void Uci::stop_search() {
    searcher_.request_stop();
    if (search_thread_.joinable()) search_thread_.join();
}

// A fixed suite of positions searched to a fixed depth. Deterministic
// (single-threaded) so CI can track nps regressions over time.
void Uci::cmd_bench(int depth) {
    static const char* kBenchFens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r1bq1rk1/pp2bppp/2n2n2/2pp4/3P4/2N1PN2/PP2BPPP/R1BQ1RK1 w - - 0 1",
        "r2q1rk1/1b1nbppp/p2ppn2/1p6/3NP3/1BN1B3/PPP1QPPP/R4RK1 w - - 0 1",
    };

    Searcher bench_searcher(64);
    bench_searcher.set_threads(1);  // deterministic single-threaded bench
    SearchLimits limits;
    limits.max_depth = depth;

    std::uint64_t total_nodes = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (const char* fen : kBenchFens) {
        Board b(fen);
        bench_searcher.new_game();
        SearchResult r = bench_searcher.search(b, limits, /*verbose=*/false);
        total_nodes += r.nodes;
    }
    double sec =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0)
            .count();
    std::uint64_t nps = sec > 0 ? (std::uint64_t)(total_nodes / sec) : total_nodes;

    std::cout << "bench depth " << depth << " nodes " << total_nodes
              << " time " << (long long)(sec * 1000) << "ms nps " << nps
              << "\n";
    std::cout.flush();
}

bool Uci::handle(const std::string& line) {
    std::istringstream ss(line);
    std::string cmd;
    ss >> cmd;
    std::string rest;
    std::getline(ss, rest);
    if (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);

    if (cmd == "uci") cmd_uci();
    else if (cmd == "isready") cmd_isready();
    else if (cmd == "ucinewgame") cmd_newgame();
    else if (cmd == "setoption") cmd_setoption(rest);
    else if (cmd == "position") cmd_position(rest);
    else if (cmd == "go") cmd_go(rest);
    else if (cmd == "stop") stop_search();
    else if (cmd == "eval") std::cout << "eval cp " << evaluate(board_) << "\n";
    else if (cmd == "perft") {
        int depth = rest.empty() ? 1 : std::stoi(rest);
        std::cout << "nodes " << perft(board_, depth) << "\n";
    }
    else if (cmd == "bench") {
        int depth = rest.empty() ? 9 : std::stoi(rest);
        cmd_bench(depth);
    }
    else if (cmd == "d") std::cout << board_.fen() << "\n";
    else if (cmd == "quit") { stop_search(); return false; }
    // Unknown commands are silently ignored, per UCI convention.
    std::cout.flush();
    return true;
}

void Uci::loop() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!handle(line)) break;
    }
}

}  // namespace shark
