#include "shark/perft.hpp"

#include "shark/movegen.hpp"

namespace shark {

std::uint64_t perft(Board& b, int depth) {
    if (depth == 0) return 1;

    MoveList list;
    generate_legal(b, list);

    if (depth == 1) return list.size();  // leaf shortcut

    std::uint64_t nodes = 0;
    for (Move m : list) {
        b.make_move(m);
        nodes += perft(b, depth - 1);
        b.unmake_move(m);
    }
    return nodes;
}

}  // namespace shark
