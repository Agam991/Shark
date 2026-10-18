#pragma once

// UCI (Universal Chess Interface) protocol driver. Turns Shark into a
// pluggable engine that any UCI GUI (Cute Chess, Arena, BanksiaGUI) or
// tournament tool (cutechess-cli) can drive -- the prerequisite for the
// Bots Royale and Stockfish Gauntlet.
//
// Reference: http://wbec-ridderkerk.nl/html/UCIProtocol.html

#include <random>
#include <string>
#include <thread>

#include "shark/board.hpp"
#include "shark/search.hpp"

namespace shark {

class Uci {
   public:
    Uci();
    ~Uci();

    // Blocking loop: reads UCI commands from stdin until "quit".
    void loop();

    // Handle a single command line (exposed for testing).
    // Returns false when the engine should quit.
    bool handle(const std::string& line);

    // Block until any in-flight search finishes naturally (test helper).
    void wait();

   private:
    void cmd_uci();
    void cmd_isready();
    void cmd_newgame();
    void cmd_setoption(const std::string& args);
    void cmd_position(const std::string& args);
    void cmd_go(const std::string& args);
    void cmd_bench(int depth);

    // Parse a UCI move ("e2e4", "e7e8q") into a legal Move for the current
    // board, or kNullMove if it isn't legal.
    Move parse_move(const std::string& uci) const;

    // Join any in-flight search thread (after requesting it to stop).
    void stop_search();

    Board board_;
    Searcher searcher_;
    std::thread search_thread_;
    std::size_t hash_mb_ = 64;
    int threads_ = 1;  // stored now; used in Phase 4 (Lazy SMP)
    bool use_book_ = true;  // personality opening repertoire on/off
    std::mt19937 rng_;      // picks among a personality's book moves
};

}  // namespace shark
