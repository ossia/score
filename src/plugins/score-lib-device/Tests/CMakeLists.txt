# Device node serialization test. Boots a MinimalApplication (via Utils.hpp),
# so it needs the APP mode (offscreen + runtime plugin discovery).
score_add_test(test_device_serialization
  SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/SerializationTest.cpp"
  APP
  PLUGINS score_lib_device score_lib_state)
