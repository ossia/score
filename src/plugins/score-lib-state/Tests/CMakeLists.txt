# Unit tests for score-lib-state. Utils.hpp boots a MinimalApplication, so APP
# supplies runtime plugin discovery and the complete static plugin link graph.
score_add_test(test_state_expression
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ExpressionParsingTests.cpp"
  APP
  PLUGINS score_lib_state)

score_add_test(test_state_serialization
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/SerializationTest.cpp"
  APP
  PLUGINS score_lib_state)

score_add_test(test_state_equality
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/EqualityTest.cpp"
  APP
  PLUGINS score_lib_state)

score_add_test(test_state_units
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/UnitTests.cpp"
  APP
  PLUGINS score_lib_state)
