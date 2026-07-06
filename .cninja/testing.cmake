# Developer build configured for running the score test suite:
# dynamic plugins (fast incremental relinks, runtime plugin discovery),
# PCH on, no unity build, debug. Enables SCORE_TESTING.
cninja_require(developer)

set_cache(CMAKE_UNITY_BUILD False)
set_cache(SCORE_TESTING True)
