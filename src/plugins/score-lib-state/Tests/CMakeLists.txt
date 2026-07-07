# Unit tests for score-lib-state. App-free (Catch2), driven by score_add_test.
score_add_test(test_state_expression
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ExpressionParsingTests.cpp"
  PLUGINS score_lib_state)

score_add_test(test_state_serialization
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/SerializationTest.cpp"
  PLUGINS score_lib_state)

score_add_test(test_state_equality
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/EqualityTest.cpp"
  PLUGINS score_lib_state)

score_add_test(test_state_units
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/UnitTests.cpp"
  PLUGINS score_lib_state)
