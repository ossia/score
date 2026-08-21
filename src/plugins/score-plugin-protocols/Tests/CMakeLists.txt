# Tests for the CAN protocol.
#
# Split in two on purpose:
#  - the DBC parser and the bit extraction are pure, portable logic and are
#    tested without a bus, a socket or an application;
#  - the round-trip test needs a real SocketCAN interface and skips itself when
#    there is none, as CI machines generally have no vcan module.
#
# Both compile DBCParser.cpp straight into the test rather than linking the
# plugin: the parser has no guard of its own precisely so that it stays
# testable, and linking score_plugin_protocols would drag the whole protocol
# stack (and a Qt/ossia runtime) into a test of arithmetic.

set(CAN_TEST_DATA "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(CAN_PARSER_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../Protocols/CAN/DBCParser.cpp")

score_add_test(test_protocol_can_dbc
  SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/DBCParserTest.cpp"
    "${CAN_PARSER_SRC}")

target_include_directories(test_protocol_can_dbc
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_definitions(test_protocol_can_dbc
  PRIVATE SCORE_CAN_TEST_DATA="${CAN_TEST_DATA}")

if(LINUX)
  score_add_test(test_protocol_can_vcan
    SOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/CANvcanTest.cpp"
      "${CAN_PARSER_SRC}"
    LIBS ossia)

  target_include_directories(test_protocol_can_vcan
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
  target_compile_definitions(test_protocol_can_vcan
    PRIVATE SCORE_CAN_TEST_DATA="${CAN_TEST_DATA}")
endif()
