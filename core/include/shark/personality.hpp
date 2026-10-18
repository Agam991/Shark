#pragma once

// Playing personalities.
//
// A personality changes HOW Shark plays, not how deep it searches:
//   * Aggressive - attacks the enemy king, values activity, plays gambits and
//                  sharp openings (King's Gambit, Evans, Najdorf, King's Indian).
//   * Defensive  - values its own king safety and a solid pawn structure, plays
//                  solid openings (London, QGD, Caro-Kann, French).
//   * Positional - values pawn structure, outposts, open files, bishop pair and
//                  slow maneuvering openings (English, Catalan, Reti, Ruy, Nimzo).
//   * Balanced   - the original evaluation, untouched (default).
//
// Each personality = (extra evaluation terms) + (small opening repertoire).
// The personality is selected with the UCI option "Personality".

#include <random>
#include <string>

#include "shark/board.hpp"

namespace shark {

enum class Personality : int {
    Balanced = 0,
    Aggressive,
    Defensive,
    Positional,
    Count
};

const char* personality_name(Personality p);

// Case-insensitive. Returns false (and leaves `out` untouched) if unknown.
bool parse_personality(const std::string& name, Personality& out);

// Global selection read by evaluate(). Only change it while no search runs.
Personality current_personality();
void set_personality(Personality p);

// Evaluation weights (centipawns). Unused for Balanced.
struct EvalWeights {
    int mobility;       // per reachable square (N, B, R, Q)
    int king_attack;    // per unit of attack danger on the enemy king   (midgame)
    int king_prox;      // per step our pieces sit closer to enemy king  (midgame)
    int shield;         // per own pawn shielding our king               (midgame)
    int king_exposure;  // penalty per unit of enemy attack on OUR king  (midgame)
    int doubled;        // penalty per doubled pawn
    int isolated;       // penalty per isolated pawn
    int passed_pct;     // passed-pawn bonus, percent of the base table
    int rook_open;      // rook on an open file
    int rook_semi;      // rook on a semi-open file
    int bishop_pair;
    int outpost;        // knight on a pawn-supported, pawn-safe outpost
    int center;         // per attack on the four central squares
    int space;          // per rank a pawn has advanced beyond the 3rd rank
};
const EvalWeights& personality_weights(Personality p);

// ---- Opening repertoire -----------------------------------------------------
// Returns a book move for `b` (keyed by position, so transpositions and FEN-only
// callers work), or kNullMove if the position isn't in this personality's book
// (always the case for Balanced).
Move personality_book_move(const Board& b, Personality p, std::mt19937& rng);

// Number of repertoire lines that contain an illegal move (should be 0; used by
// the unit tests to keep the book honest).
int personality_book_bad_lines();

}  // namespace shark
