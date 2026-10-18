# ---- Shark tests --------------------------------------------------------

add_executable(shark_tests
    test_main.cpp
    sanity_test.cpp
    perft_test.cpp
    tactic_test.cpp
    uci_test.cpp
    personality_test.cpp
)
target_link_libraries(shark_tests PRIVATE shark_core)

add_test(NAME shark_tests COMMAND shark_tests)
