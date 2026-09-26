# Tests for the `.midimap.json` device-description reader. Compiled into the
# test like the MIDI Guide reader below: it is free of Qt and ossia, and only
# needs rapidjson, which is header-only.
score_add_test(test_protocol_midi_device_map
  SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/MidiDeviceMapTest.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../Protocols/MIDIDevices/MidiDeviceMap.cpp")

target_include_directories(test_protocol_midi_device_map
  PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_include_directories(test_protocol_midi_device_map
  SYSTEM PRIVATE "${OSSIA_3RDPARTY_FOLDER}/rapidjson/include")

# Standing a tree up needs the real backends, so this links ossia and reports
# what the machine could not do rather than failing.
if(OSSIA_PROTOCOL_MIDI)
  score_add_test(test_protocol_midi_device_protocol
    SOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/MidiDeviceProtocolTest.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/../Protocols/MIDIDevices/MidiDeviceProtocol.cpp"
      "${CMAKE_CURRENT_SOURCE_DIR}/../Protocols/MIDIDevices/MidiDeviceMap.cpp"
    LIBS ossia ${QT_PREFIX}::Core)

  target_include_directories(test_protocol_midi_device_protocol
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
  target_include_directories(test_protocol_midi_device_protocol
    SYSTEM PRIVATE "${OSSIA_3RDPARTY_FOLDER}/rapidjson/include")
endif()

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

# The Bitfocus host against a mock companion module. Skips without node.
if(UNIX AND NOT EMSCRIPTEN)
  set(BITFOCUS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../Protocols/Bitfocus")
  score_add_test(test_protocol_bitfocus_host
    SOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/BitfocusHostTest.cpp"
      "${BITFOCUS_DIR}/BitfocusContext.cpp"
      "${BITFOCUS_DIR}/BitfocusContext.unix.cpp"
      "${BITFOCUS_DIR}/BitfocusProtocol.cpp"
    LIBS ossia ${QT_PREFIX}::Network)
  target_include_directories(test_protocol_bitfocus_host
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
  target_compile_definitions(test_protocol_bitfocus_host
    PRIVATE SCORE_BITFOCUS_MOCK_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/bitfocus-mock")

  # Drives a real companion module through score's Bitfocus host; not a ctest
  # entry, see tests/tools/bitfocus/sweep.py.
  score_add_test(bitfocus_module_driver NO_CTEST
    SOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/BitfocusModuleDriver.cpp"
      "${BITFOCUS_DIR}/BitfocusContext.cpp"
      "${BITFOCUS_DIR}/BitfocusContext.unix.cpp"
      "${BITFOCUS_DIR}/BitfocusProtocol.cpp"
      "${BITFOCUS_DIR}/BitfocusProtocolSettingsWidget.cpp"
      "${BITFOCUS_DIR}/BitfocusSpecificSettingsSerialization.cpp"
    PLUGINS score_lib_state score_lib_device score_plugin_deviceexplorer
    LIBS ossia ${QT_PREFIX}::Widgets ${QT_PREFIX}::Qml)
  target_include_directories(bitfocus_module_driver
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/.." "${SCORE_ROOT_SOURCE_DIR}/tests/fixtures")
  target_compile_definitions(bitfocus_module_driver
    PRIVATE SCORE_TEST_BINARY_DIR="${SCORE_ROOT_BINARY_DIR}")
endif()
