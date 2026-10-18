# ---- Shark engine core --------------------------------------------------

# Reusable engine logic lives in a library so both the executable and the
# tests can link against it (DRY: one source of truth for engine code).

set(SHARK_LIB_SOURCES
    src/bitboard.cpp
    src/board.cpp
    src/movegen.cpp
    src/perft.cpp
    src/zobrist.cpp
    src/eval.cpp
    src/personality.cpp
    src/tt.cpp
    src/search.cpp
    src/uci.cpp
)

add_library(shark_core ${SHARK_LIB_SOURCES})
target_include_directories(shark_core PUBLIC include)

find_package(Threads REQUIRED)
target_link_libraries(shark_core PUBLIC Threads::Threads)

# ---- Engine executable ------------------------------------------------------
add_executable(engine src/engine.cpp)
target_link_libraries(engine PRIVATE shark_core)

# Keep the binary at core/engine so the existing web/server.js keeps working
# during the transition (it spawns ../core/engine).
set_target_properties(engine PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
foreach(CFG Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${CFG} CFG_UPPER)
    set_target_properties(engine PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${CFG_UPPER} ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
# ---- Tests ------------------------------------------------------------------
add_subdirectory(tests)
