#pragma once

// Search: iterative-deepening negamax with alpha-beta pruning, quiescence,
// transposition table, and move ordering (TT move, MVV-LVA, killers, history).
//
// Multithreading (Phase 4): Lazy SMP. N worker threads each run their own
// iterative-deepening search on a private board copy, sharing one lockless
// transposition table. A single atomic stop flag coordinates shutdown. Thread
// count defaults to 1 and is hard-capped at the machine's core count.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

#include "shark/board.hpp"
#include "shark/tt.hpp"
#include "shark/types.hpp"

namespace shark {

struct SearchLimits {
    int max_depth = 64;            // hard depth cap
    std::int64_t movetime_ms = 0;  // 0 = no time limit (depth-limited)
    std::uint64_t max_nodes = 0;   // 0 = unlimited
};

struct SearchResult {
    Move best = kNullMove;
    int score = 0;
    int depth = 0;
    std::uint64_t nodes = 0;
};

class Searcher {
   public:
    explicit Searcher(std::size_t tt_mb = 64) : tt_(tt_mb) {}

    void set_tt_size(std::size_t mb) { tt_.resize(mb); }
    void set_threads(int n);  // clamped to [1, hardware_concurrency]
    int threads() const { return threads_; }
    void new_game() { tt_.clear(); }

    // Cooperatively abort an in-flight search (used by UCI "stop"/"quit").
    void request_stop() { stop_.store(true, std::memory_order_relaxed); }

    SearchResult search(Board& b, const SearchLimits& limits,
                        bool verbose = true);

   private:
    static constexpr int kMaxPly = 128;

    // Per-thread search state (private board + heuristics, shared TT via s_).
    struct Worker {
        Searcher* s = nullptr;
        Board board;
        std::uint64_t nodes = 0;
        Move root_best = kNullMove;
        int root_score = 0;
        int completed_depth = 0;
        Move killers[kMaxPly][2] = {};
        int history[COLOR_NB][SQ_NB][SQ_NB] = {};

        void run(const SearchLimits& limits, bool verbose, bool is_main);
        int negamax(int depth, int alpha, int beta, int ply, bool can_null = true);
        int quiescence(int alpha, int beta, int ply);
        std::string pv_string(int max_len);
    };

    bool time_up();

    TranspositionTable tt_;
    int threads_ = 1;
    std::atomic<bool> stop_{false};
    std::atomic<std::uint64_t> total_nodes_{0};

    std::chrono::steady_clock::time_point start_;
    std::int64_t movetime_ms_ = 0;
    std::uint64_t max_nodes_ = 0;
};

}  // namespace shark
