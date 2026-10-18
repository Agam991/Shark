#include "shark/search.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "shark/eval.hpp"
#include "shark/movegen.hpp"

namespace shark {

namespace {

constexpr int kInf = kValueInfinite;
constexpr int kPieceVal[6] = {100, 320, 330, 500, 900, 20000};

bool is_capture(const Board& b, Move m) {
    return !b.piece_on(m.to()).is_none() || m.type() == MT_EN_PASSANT;
}

int score_to_tt(int score, int ply) {
    if (score >= kValueMateInMaxPly) return score + ply;
    if (score <= -kValueMateInMaxPly) return score - ply;
    return score;
}
int score_from_tt(int score, int ply) {
    if (score >= kValueMateInMaxPly) return score - ply;
    if (score <= -kValueMateInMaxPly) return score + ply;
    return score;
}

}  // namespace

// ---- Thread / time management ----------------------------------------------
void Searcher::set_threads(int n) {
    int hw = static_cast<int>(std::thread::hardware_concurrency());
    if (hw < 1) hw = 1;
    threads_ = std::clamp(n, 1, hw);  // never exceed physical cores
}

bool Searcher::time_up() {
    if (max_nodes_ && total_nodes_.load(std::memory_order_relaxed) >= max_nodes_)
        return true;
    if (movetime_ms_ <= 0) return false;
    auto now = std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_)
                  .count();
    return ms >= movetime_ms_;
}

// ---- Quiescence -------------------------------------------------------------
int Searcher::Worker::quiescence(int alpha, int beta, int ply) {
    if ((++nodes & 2047) == 0) {
        s->total_nodes_.fetch_add(2048, std::memory_order_relaxed);
        if (s->time_up()) s->stop_.store(true, std::memory_order_relaxed);
    }
    if (s->stop_.load(std::memory_order_relaxed)) return 0;

    int stand_pat = evaluate(board);
    if (stand_pat >= beta) return beta;
    if (stand_pat > alpha) alpha = stand_pat;
    if (ply >= kMaxPly - 1) return stand_pat;

    MoveList moves;
    generate_legal(board, moves);

    struct Scored { Move m; int s; };
    Scored scored[256];
    int n = 0;
    for (Move m : moves) {
        bool cap = is_capture(board, m);
        bool promo = m.type() == MT_PROMOTION;
        if (!cap && !promo) continue;
        int sc = 0;
        if (cap) {
            PieceType victim = (m.type() == MT_EN_PASSANT)
                                   ? PAWN
                                   : board.piece_on(m.to()).type;
            PieceType attacker = board.piece_on(m.from()).type;
            sc = kPieceVal[victim] * 16 - kPieceVal[attacker];
        }
        if (promo) sc += kPieceVal[m.promotion()];
        scored[n++] = {m, sc};
    }

    for (int i = 0; i < n; ++i) {
        int best = i;
        for (int j = i + 1; j < n; ++j)
            if (scored[j].s > scored[best].s) best = j;
        std::swap(scored[i], scored[best]);

        board.make_move(scored[i].m);
        int score = -quiescence(-beta, -alpha, ply + 1);
        board.unmake_move(scored[i].m);

        if (s->stop_.load(std::memory_order_relaxed)) return 0;
        if (score >= beta) return beta;
        if (score > alpha) alpha = score;
    }
    return alpha;
}

// ---- Negamax ----------------------------------------------------------------
int Searcher::Worker::negamax(int depth, int alpha, int beta, int ply,
                              bool can_null) {
    if ((++nodes & 2047) == 0) {
        s->total_nodes_.fetch_add(2048, std::memory_order_relaxed);
        if (s->time_up()) s->stop_.store(true, std::memory_order_relaxed);
    }
    if (s->stop_.load(std::memory_order_relaxed)) return 0;

    bool in_check = board.in_check();
    if (in_check) ++depth;  // check extension
    if (depth <= 0) return quiescence(alpha, beta, ply);

    bool is_pv = (beta - alpha) > 1;  // full window => principal-variation node

    std::uint64_t key = board.key();
    TTData tte = s->tt_.probe(key);
    Move tt_move = kNullMove;
    if (tte.hit) {
        tt_move = tte.move;
        if (ply > 0 && tte.depth >= depth) {
            int sc = score_from_tt(tte.score, ply);
            if (tte.bound == BOUND_EXACT) return sc;
            if (tte.bound == BOUND_LOWER && sc >= beta) return sc;
            if (tte.bound == BOUND_UPPER && sc <= alpha) return sc;
        }
    }

    // ---- Null-move pruning ---------------------------------------------------
    // If we can pass the turn and STILL be winning (beta cutoff) after a
    // reduced search, this node is too good to be relevant -> prune. Skipped
    // in check, in PV nodes, and in likely-zugzwang (no non-pawn material).
    if (can_null && !is_pv && !in_check && depth >= 3 &&
        board.has_non_pawn_material(board.side_to_move())) {
        int R = 2 + depth / 4;  // reduction
        board.make_null_move();
        int null_score =
            -negamax(depth - 1 - R, -beta, -beta + 1, ply + 1, /*can_null=*/false);
        board.unmake_null_move();
        if (s->stop_.load(std::memory_order_relaxed)) return 0;
        if (null_score >= beta) return beta;  // fail-high: prune
    }

    MoveList moves;
    generate_legal(board, moves);
    if (moves.size() == 0) return in_check ? (-kValueMate + ply) : 0;

    Color us = board.side_to_move();

    struct Scored { Move m; int s; };
    Scored scored[256];
    int n = 0;
    for (Move m : moves) {
        int sc;
        if (m == tt_move) sc = 2'000'000;
        else if (is_capture(board, m)) {
            PieceType victim = (m.type() == MT_EN_PASSANT)
                                   ? PAWN
                                   : board.piece_on(m.to()).type;
            PieceType attacker = board.piece_on(m.from()).type;
            sc = 1'000'000 + kPieceVal[victim] * 16 - kPieceVal[attacker];
        } else if (m.type() == MT_PROMOTION) {
            sc = 900'000 + kPieceVal[m.promotion()];
        } else if (m == killers[ply][0]) sc = 800'000;
        else if (m == killers[ply][1]) sc = 700'000;
        else sc = history[us][m.from()][m.to()];
        scored[n++] = {m, sc};
    }

    int best = -kInf;
    Move best_move = kNullMove;
    Bound flag = BOUND_UPPER;

    for (int i = 0; i < n; ++i) {
        int pick = i;
        for (int j = i + 1; j < n; ++j)
            if (scored[j].s > scored[pick].s) pick = j;
        std::swap(scored[i], scored[pick]);
        Move m = scored[i].m;

        bool quiet = !is_capture(board, m) && m.type() != MT_PROMOTION;

        board.make_move(m);
        bool gives_check = board.in_check();

        int score;
        if (i == 0) {
            // Search the first (best-ordered) move with a full window.
            score = -negamax(depth - 1, -beta, -alpha, ply + 1);
        } else {
            // ---- Late Move Reductions ---------------------------------------
            // Quiet, late moves that don't give check are searched shallower
            // with a null window; only re-searched fully if they surprise us.
            int reduction = 0;
            if (depth >= 3 && i >= 3 && quiet && !gives_check && !in_check) {
                reduction = 1 + (i >= 6 ? 1 : 0);
            }
            score = -negamax(depth - 1 - reduction, -alpha - 1, -alpha,
                             ply + 1);
            // Re-search at full depth/window if it beat alpha.
            if (score > alpha && (reduction > 0 || score < beta)) {
                score = -negamax(depth - 1, -beta, -alpha, ply + 1);
            }
        }

        board.unmake_move(m);

        if (s->stop_.load(std::memory_order_relaxed)) return 0;

        if (score > best) {
            best = score;
            best_move = m;
            if (ply == 0) { root_best = m; root_score = score; }
            if (score > alpha) {
                alpha = score;
                flag = BOUND_EXACT;
                if (alpha >= beta) {
                    flag = BOUND_LOWER;
                    if (quiet) {
                        if (m != killers[ply][0]) {
                            killers[ply][1] = killers[ply][0];
                            killers[ply][0] = m;
                        }
                        history[us][m.from()][m.to()] += depth * depth;
                    }
                    break;
                }
            }
        }
    }

    s->tt_.store(key, score_to_tt(best, ply), flag, depth, best_move);
    return best;
}

// ---- PV extraction ----------------------------------------------------------
std::string Searcher::Worker::pv_string(int max_len) {
    std::string pv;
    std::vector<Move> played;
    for (int i = 0; i < max_len; ++i) {
        TTData e = s->tt_.probe(board.key());
        if (!e.hit || e.move == kNullMove) break;

        MoveList legal;
        generate_legal(board, legal);
        bool ok = false;
        for (Move m : legal)
            if (m == e.move) { ok = true; break; }
        if (!ok) break;

        if (!pv.empty()) pv += ' ';
        pv += e.move.to_uci();
        board.make_move(e.move);
        played.push_back(e.move);
    }
    for (auto it = played.rbegin(); it != played.rend(); ++it)
        board.unmake_move(*it);
    return pv;
}

// ---- Worker driver (one iterative-deepening loop) ---------------------------
void Searcher::Worker::run(const SearchLimits& limits, bool verbose,
                           bool is_main) {
    for (int depth = 1;
         depth <= limits.max_depth && !s->stop_.load(std::memory_order_relaxed);
         ++depth) {
        int score = negamax(depth, -kInf, kInf, 0);
        if (s->stop_.load(std::memory_order_relaxed)) break;  // incomplete

        completed_depth = depth;
        root_score = score;

        if (is_main && verbose) {
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - s->start_)
                          .count();
            std::uint64_t tot = s->total_nodes_.load(std::memory_order_relaxed);
            std::uint64_t nps = ms > 0 ? (tot * 1000ULL / (std::uint64_t)ms) : tot;
            std::string pv = pv_string(depth);
            if (pv.empty()) pv = root_best.to_uci();

            char score_buf[32];
            if (score >= kValueMateInMaxPly)
                std::snprintf(score_buf, sizeof(score_buf), "mate %d",
                              (kValueMate - score + 1) / 2);
            else if (score <= -kValueMateInMaxPly)
                std::snprintf(score_buf, sizeof(score_buf), "mate %d",
                              -(kValueMate + score + 1) / 2);
            else
                std::snprintf(score_buf, sizeof(score_buf), "cp %d", score);

            std::cout << "info depth " << depth << " score " << score_buf
                      << " nodes " << tot << " nps " << nps << " time " << ms
                      << " pv " << pv << "\n";
            std::cout.flush();
        }

        if (is_main &&
            (score >= kValueMateInMaxPly || score <= -kValueMateInMaxPly))
            break;  // proven mate
    }

    // The main worker dictates termination: once it's done, helpers wind down.
    // This guarantees helper threads never outlive the main search (no runaway
    // CPU burn).
    if (is_main) s->stop_.store(true, std::memory_order_relaxed);
}

// ---- Public entry -----------------------------------------------------------
SearchResult Searcher::search(Board& b, const SearchLimits& limits,
                              bool verbose) {
    stop_.store(false, std::memory_order_relaxed);
    total_nodes_.store(0, std::memory_order_relaxed);
    start_ = std::chrono::steady_clock::now();
    movetime_ms_ = limits.movetime_ms;
    max_nodes_ = limits.max_nodes;

    SearchResult result;
    MoveList root_moves;
    generate_legal(b, root_moves);
    if (root_moves.size() == 0) return result;
    result.best = root_moves[0];

    int n = threads_;

    // Build workers, each with its own board copy.
    std::vector<std::unique_ptr<Worker>> workers;
    workers.reserve(n);
    for (int i = 0; i < n; ++i) {
        auto w = std::make_unique<Worker>();
        w->s = this;
        w->board = b;
        w->root_best = result.best;  // safe fallback
        workers.push_back(std::move(w));
    }

    // Launch helper threads (1..n-1); run the main worker on this thread so its
    // verbose output is inline.
    std::vector<std::thread> pool;
    pool.reserve(n - 1);
    for (int i = 1; i < n; ++i) {
        Worker* w = workers[i].get();
        pool.emplace_back([w, &limits]() {
            w->run(limits, /*verbose=*/false, /*is_main=*/false);
        });
    }
    workers[0]->run(limits, verbose, /*is_main=*/true);

    for (auto& t : pool) t.join();

    // Pick the deepest completed result (ties -> main thread).
    Worker* best_w = workers[0].get();
    for (int i = 1; i < n; ++i)
        if (workers[i]->completed_depth > best_w->completed_depth)
            best_w = workers[i].get();

    if (!best_w->root_best.is_null()) result.best = best_w->root_best;
    result.score = best_w->root_score;
    result.depth = best_w->completed_depth;
    result.nodes = total_nodes_.load(std::memory_order_relaxed);
    return result;
}

}  // namespace shark
