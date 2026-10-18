#pragma once

// Transposition table: caches search results keyed by Zobrist hash so we don't
// re-search positions reached by different move orders (transpositions).
//
// Thread-safe via Hyatt's lockless XOR scheme: each slot stores {key ^ data,
// data}. A concurrent torn write is detected on probe because (stored_key ^
// data) will no longer equal the real key. Uses relaxed atomics (defined
// behavior; benign races only).

#include <atomic>
#include <cstdint>
#include <vector>

#include "shark/types.hpp"

namespace shark {

enum Bound : std::uint8_t {
    BOUND_NONE = 0,
    BOUND_UPPER = 1,  // fail-low: score is an upper bound (alpha)
    BOUND_LOWER = 2,  // fail-high: score is a lower bound (beta)
    BOUND_EXACT = 3
};

// Result of a probe (a value copy, since the slot may change under us).
struct TTData {
    bool hit = false;
    Move move = kNullMove;
    int score = 0;
    int depth = -1;
    Bound bound = BOUND_NONE;
};

class TranspositionTable {
   public:
    explicit TranspositionTable(std::size_t mb = 64) { resize(mb); }

    void resize(std::size_t mb);
    void clear();

    TTData probe(std::uint64_t key) const;
    void store(std::uint64_t key, int score, Bound bound, int depth, Move move);

   private:
    struct Slot {
        std::atomic<std::uint64_t> key{0};   // real_key ^ data
        std::atomic<std::uint64_t> data{0};  // packed payload
    };

    static std::uint64_t pack(Move m, int score, int depth, Bound b);
    std::size_t index(std::uint64_t key) const { return key & mask_; }

    std::vector<Slot> table_;
    std::size_t mask_ = 0;
};

}  // namespace shark
