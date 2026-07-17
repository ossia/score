# Root of the score test tree. Included from the top-level CMakeLists.txt
# only when SCORE_TESTING is ON.
score_setup_catch2()

add_subdirectory(fixtures)
add_subdirectory(unit)
add_subdirectory(integration)
add_subdirectory(nodes)
add_subdirectory(hardware)
