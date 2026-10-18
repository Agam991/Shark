#include "shark/tt.hpp"

#include <bit>

namespace shark {

// Payload packing (64 bits):
//   bits  0-15 : move (16-bit packed Move)
//   bits 16-31 : score (int16)
//   bits 32-39 : depth (int8)
//   bits 40-41 : bound
std::uint64_t TranspositionTable::pack(Move m, int score, int depth, Bound b) {
    std::uint16_t mv = m.raw();
    std::uint16_t sc = static_cast<std::uint16_t>(static_cast<std::int16_t>(score));
    std::uint8_t dp = static_cast<std::uint8_t>(static_cast<std::int8_t>(depth));
    std::uint64_t data = mv;
    data |= static_cast<std::uint64_t>(sc) << 16;
    data |= static_cast<std::uint64_t>(dp) << 32;
    data |= static_cast<std::uint64_t>(b) << 40;
    return data;
}

void TranspositionTable::resize(std::size_t mb) {
    std::size_t bytes = mb * 1024 * 1024;
    std::size_t count = bytes / sizeof(Slot);
    if (count < 1024) count = 1024;
    std::size_t pow2 = std::bit_floor(count);  // power of two for mask indexing
    table_ = std::vector<Slot>(pow2);          // value-inits atomics to 0
    mask_ = pow2 - 1;
}

void TranspositionTable::clear() {
    for (auto& s : table_) {
        s.key.store(0, std::memory_order_relaxed);
        s.data.store(0, std::memory_order_relaxed);
    }
}

TTData TranspositionTable::probe(std::uint64_t key) const {
    const Slot& s = table_[index(key)];
    std::uint64_t k = s.key.load(std::memory_order_relaxed);
    std::uint64_t d = s.data.load(std::memory_order_relaxed);

    TTData out;
    if ((k ^ d) != key) return out;  // empty or torn write -> miss

    out.hit = true;
    out.move = Move(static_cast<std::uint16_t>(d & 0xFFFF));
    out.score = static_cast<std::int16_t>((d >> 16) & 0xFFFF);
    out.depth = static_cast<std::int8_t>((d >> 32) & 0xFF);
    out.bound = static_cast<Bound>((d >> 40) & 0x3);
    return out;
}

void TranspositionTable::store(std::uint64_t key, int score, Bound bound,
                               int depth, Move move) {
    Slot& s = table_[index(key)];

    // Depth-preferred replacement: keep deeper analysis unless this is an exact
    // score or the slot holds a different position. (Approximate under races,
    // which is fine for a TT.)
    std::uint64_t old_k = s.key.load(std::memory_order_relaxed);
    std::uint64_t old_d = s.data.load(std::memory_order_relaxed);
    if ((old_k ^ old_d) == key) {
        int old_depth = static_cast<std::int8_t>((old_d >> 32) & 0xFF);
        if (old_depth > depth && bound != BOUND_EXACT) return;
        // Preserve a previous best move if this store has none.
        if (move == kNullMove)
            move = Move(static_cast<std::uint16_t>(old_d & 0xFFFF));
    }

    std::uint64_t data = pack(move, score, depth, bound);
    // Write data first, then the XOR'd key, so a concurrent probe either sees
    // the fully-formed pair or fails the (key ^ data) check.
    s.data.store(data, std::memory_order_relaxed);
    s.key.store(key ^ data, std::memory_order_relaxed);
}

}  // namespace shark
