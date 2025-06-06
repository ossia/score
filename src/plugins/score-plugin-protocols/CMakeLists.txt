project(score_plugin_protocols LANGUAGES CXX)

score_common_setup()

# Packages
find_package(${QT_VERSION} REQUIRED COMPONENTS Core Widgets Network)
find_package(${QT_VERSION} OPTIONAL_COMPONENTS Qml)

# Files & main target
set(HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Settings/Model.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Settings/Presenter.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Settings/View.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Settings/Factory.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSSIADevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/ProtocolLibrary.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/RateWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/LibraryDeviceEnumerator.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/DNSSDDeviceEnumerator.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_protocols.hpp"
)

set(SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Settings/Model.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Settings/Presenter.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Settings/View.cpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/LibraryDeviceEnumerator.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/DNSSDDeviceEnumerator.cpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_protocols.cpp"
)

set(OSC_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCSpecificSettings.hpp"
)
set(OSC_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCSpecificSettingsSerialization.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSC/OSCProtocolFactory.cpp"
)

set(MQTT_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MQTT/MQTTDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MQTT/MQTTProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MQTT/MQTTProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MQTT/MQTTSpecificSettings.hpp"
)
set(MQTT_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MQTT/MQTTSpecificSettingsSerialization.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MQTT/MQTTProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MQTT/MQTTDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MQTT/MQTTProtocolFactory.cpp"
)

set(COAP_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/CoAP/CoAPDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/CoAP/CoAPProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/CoAP/CoAPProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/CoAP/CoAPSpecificSettings.hpp"
)
set(COAP_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/CoAP/CoAPSpecificSettingsSerialization.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/CoAP/CoAPProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/CoAP/CoAPDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/CoAP/CoAPProtocolFactory.cpp"
)

set(MINUIT_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitSpecificSettings.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitProtocolFactory.hpp"
)
set(MINUIT_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitSpecificSettingsSerialization.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Minuit/MinuitProtocolFactory.cpp"
)

set(OSCQUERY_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSCQuery/OSCQueryProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSCQuery/OSCQueryDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSCQuery/OSCQuerySpecificSettings.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSCQuery/OSCQueryProtocolFactory.hpp"
)
set(OSCQUERY_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSCQuery/OSCQuerySpecificSettingsSerialization.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSCQuery/OSCQueryProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSCQuery/OSCQueryDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSCQuery/OSCQueryProtocolFactory.cpp"
)

set(MIDI_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDISpecificSettings.hpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MCU/MCUProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MCU/MCUDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MCU/MCUProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MCU/MCUSpecificSettings.hpp"
)
set(MIDI_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDISpecificSettingsSerialization.cpp"

  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MCU/MCUProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MCU/MCUDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MCU/MCUProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MCU/MCUSpecificSettingsSerialization.cpp"
)

set(HTTP_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/HTTP/HTTPProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/HTTP/HTTPDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/HTTP/HTTPProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/HTTP/HTTPSpecificSettings.hpp"
)
set(HTTP_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/HTTP/HTTPProtocolSettingsWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/HTTP/HTTPDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/HTTP/HTTPProtocolFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/HTTP/HTTPSpecificSettingsSerialization.cpp"
)

set(WS_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/WS/WSProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/WS/WSDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/WS/WSProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/WS/WSSpecificSettings.hpp"
)
set(WS_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/WS/WSProtocolSettingsWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/WS/WSDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/WS/WSProtocolFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/WS/WSSpecificSettingsSerialization.cpp"
)

set(SERIAL_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Serial/SerialProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Serial/SerialDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Serial/SerialProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Serial/SerialSpecificSettings.hpp"
)
set(SERIAL_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Serial/SerialProtocolSettingsWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Serial/SerialDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Serial/SerialProtocolFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Serial/SerialSpecificSettingsSerialization.cpp"
)

set(PHIDGETS_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Phidgets/PhidgetsProtocolFactory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Phidgets/PhidgetsDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Phidgets/PhidgetsProtocolSettingsWidget.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Phidgets/PhidgetsSpecificSettings.hpp"
)
set(PHIDGETS_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Phidgets/PhidgetsProtocolSettingsWidget.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Phidgets/PhidgetsDevice.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Phidgets/PhidgetsProtocolFactory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Phidgets/PhidgetsSpecificSettingsSerialization.cpp"
)

set(JOYSTICK_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Joystick/JoystickDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Joystick/JoystickProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Joystick/JoystickProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Joystick/JoystickSpecificSettings.hpp"
)

set(JOYSTICK_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Joystick/JoystickDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Joystick/JoystickProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Joystick/JoystickProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Joystick/JoystickSpecificSettingsSerialization.cpp"
)

set(WIIMOTE_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Wiimote/WiimoteDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Wiimote/WiimoteProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Wiimote/WiimoteProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Wiimote/WiimoteSpecificSettings.hpp"
)

set(WIIMOTE_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Wiimote/WiimoteDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Wiimote/WiimoteProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Wiimote/WiimoteProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Wiimote/WiimoteSpecificSettingsSerialization.cpp"
)


set(ARTNET_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/ArtnetDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/ArtnetProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/ArtnetProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/ArtnetSpecificSettings.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/DMXFixtureInstantiation.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/DMXProtocolCreation.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/FixtureDatabase.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/FixtureDialog.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/LEDDialog.hpp"
)

set(ARTNET_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/ArtnetDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/ArtnetProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/ArtnetProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/ArtnetSpecificSettingsSerialization.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/DMXFixtureInstantiation.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/DMXProtocolCreation.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/FixtureDatabase.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/FixtureDialog.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/LEDDialog.cpp"
)

set(SIMPLEIO_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/SimpleIO/SimpleIODevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/SimpleIO/SimpleIOProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/SimpleIO/SimpleIOProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/SimpleIO/SimpleIOSpecificSettings.hpp"
)

set(SIMPLEIO_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/SimpleIO/SimpleIODevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/SimpleIO/SimpleIOProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/SimpleIO/SimpleIOProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/SimpleIO/SimpleIOSpecificSettingsSerialization.cpp"
)

set(GPS_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/GPS/GPSDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/GPS/GPSProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/GPS/GPSProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/GPS/GPSSpecificSettings.hpp"
)

set(GPS_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/GPS/GPSDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/GPS/GPSProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/GPS/GPSProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/GPS/GPSSpecificSettingsSerialization.cpp"
)

set(EVDEV_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Evdev/EvdevDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Evdev/EvdevProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Evdev/EvdevProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Evdev/EvdevSpecificSettings.hpp"
)

set(EVDEV_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Evdev/EvdevDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Evdev/EvdevProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Evdev/EvdevProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Evdev/EvdevSpecificSettingsSerialization.cpp"
)

set(BITFOCUS_HDRS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusContext.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusEnumerator.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusProtocolFactory.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusProtocolSettingsWidget.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusSpecificSettings.hpp"
)

set(BITFOCUS_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusContext.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusEnumerator.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusSpecificSettingsSerialization.cpp"
)



set(MAPPER_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Mapper/MapperDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Mapper/MapperDevice.cpp"
)

set(LIBMAPPER_CLIENT_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Libmapper/LibmapperClientDevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Libmapper/LibmapperClientDevice.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

if(OSSIA_PROTOCOL_OSC)
    target_sources(${PROJECT_NAME} PRIVATE ${OSC_HDRS} ${OSC_SRCS})
    list(APPEND SCORE_FEATURES_LIST protocol_osc)
endif()

if(OSSIA_PROTOCOL_MINUIT)
    target_sources(${PROJECT_NAME} PRIVATE ${MINUIT_HDRS} ${MINUIT_SRCS})
    list(APPEND SCORE_FEATURES_LIST protocol_minuit)
endif()

if(OSSIA_PROTOCOL_OSCQUERY)
  target_sources(${PROJECT_NAME} PRIVATE ${OSCQUERY_HDRS} ${OSCQUERY_SRCS})
  list(APPEND SCORE_FEATURES_LIST protocol_oscquery)
endif()

if(OSSIA_PROTOCOL_MIDI)
    target_sources(${PROJECT_NAME} PRIVATE ${MIDI_HDRS} ${MIDI_SRCS})
    list(APPEND SCORE_FEATURES_LIST protocol_midi)
endif()

if(OSSIA_PROTOCOL_HTTP)
    target_sources(${PROJECT_NAME} PRIVATE ${HTTP_HDRS} ${HTTP_SRCS})
    list(APPEND SCORE_FEATURES_LIST protocol_http)
endif()

if(OSSIA_PROTOCOL_WEBSOCKETS)
    target_sources(${PROJECT_NAME} PRIVATE ${WS_HDRS} ${WS_SRCS})
    list(APPEND SCORE_FEATURES_LIST protocol_ws)
endif()

if(OSSIA_PROTOCOL_LIBMAPPER)
  target_sources(${PROJECT_NAME} PRIVATE ${LIBMAPPER_CLIENT_SRCS})
  target_link_libraries(${PROJECT_NAME} PRIVATE mapper)
  list(APPEND SCORE_FEATURES_LIST protocol_libmapper)
endif()

if(OSSIA_PROTOCOL_SERIAL)
    target_sources(${PROJECT_NAME} PRIVATE ${SERIAL_HDRS} ${SERIAL_SRCS})
    find_package(${QT_VERSION} REQUIRED COMPONENTS SerialPort)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${QT_PREFIX}::SerialPort)
    list(APPEND SCORE_FEATURES_LIST protocol_serial)
endif()

if(OSSIA_PROTOCOL_PHIDGETS)
    target_sources(${PROJECT_NAME} PRIVATE ${PHIDGETS_HDRS} ${PHIDGETS_SRCS})
    list(APPEND SCORE_FEATURES_LIST protocol_phidgets)
endif()

if(OSSIA_PROTOCOL_JOYSTICK)
    target_sources(${PROJECT_NAME} PRIVATE ${JOYSTICK_HDRS} ${JOYSTICK_SRCS})
    list(APPEND SCORE_FEATURES_LIST protocol_joystick)
endif()

if(OSSIA_PROTOCOL_LEAPMOTION)
  target_sources(${PROJECT_NAME} PRIVATE ${LEAPMOTION_HDRS} ${LEAPMOTION_SRCS})
  list(APPEND SCORE_FEATURES_LIST protocol_leapmotion)
endif()

if(OSSIA_PROTOCOL_WIIMOTE)
    target_sources(${PROJECT_NAME} PRIVATE ${WIIMOTE_HDRS} ${WIIMOTE_SRCS})
    list(APPEND SCORE_FEATURES_LIST protocol_wiimote)
endif()

if(OSSIA_PROTOCOL_MQTT5)
    target_sources(${PROJECT_NAME} PRIVATE ${MQTT_HDRS} ${MQTT_SRCS})
    list(APPEND SCORE_FEATURES_LIST protocol_mqtt)
endif()

if(OSSIA_PROTOCOL_COAP)
    target_sources(${PROJECT_NAME} PRIVATE ${COAP_HDRS} ${COAP_SRCS})
    list(APPEND SCORE_FEATURES_LIST protocol_coap)
    target_compile_definitions(${PROJECT_NAME} PRIVATE OSSIA_PROTOCOL_COAP)
endif()

if(OSSIA_PROTOCOL_ARTNET)
  # Needed for listing serial ports for DMX USB
  find_package(${QT_VERSION} REQUIRED COMPONENTS SerialPort)
  if(TARGET "${QT_PREFIX}::SerialPort")
    target_sources(${PROJECT_NAME} PRIVATE ${ARTNET_HDRS} ${ARTNET_SRCS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${QT_PREFIX}::SerialPort)
    list(APPEND SCORE_FEATURES_LIST protocol_artnet)
  endif()
endif()

find_path(LINUX_HEADERS_INCLUDE_DIR linux/gpio.h)
if(LINUX_HEADERS_INCLUDE_DIR AND UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
  target_sources(${PROJECT_NAME} PRIVATE ${SIMPLEIO_HDRS} ${SIMPLEIO_SRCS})
  target_compile_definitions(${PROJECT_NAME} PRIVATE OSSIA_PROTOCOL_SIMPLEIO)
  target_link_libraries(${PROJECT_NAME} PRIVATE simpleio)
  list(APPEND SCORE_FEATURES_LIST protocol_simpleio)
endif()

find_path(GPS_HEADER gps.h)
if(GPS_HEADER)
  target_sources(${PROJECT_NAME} PRIVATE ${GPS_HDRS} ${GPS_SRCS})
  target_include_directories(${PROJECT_NAME} PRIVATE ${GPS_HEADER})
  target_compile_definitions(${PROJECT_NAME} PRIVATE OSSIA_PROTOCOL_GPS)
  list(APPEND SCORE_FEATURES_LIST protocol_gps)
endif()

if(LINUX)
  target_sources(${PROJECT_NAME} PRIVATE ${EVDEV_HDRS} ${EVDEV_SRCS})
  target_include_directories(${PROJECT_NAME} PRIVATE ${EVDEV_HEADER})
  target_compile_definitions(${PROJECT_NAME} PRIVATE OSSIA_PROTOCOL_EVDEV)
  list(APPEND SCORE_FEATURES_LIST protocol_evdev)
endif()

if(NOT EMSCRIPTEN)
  target_sources(${PROJECT_NAME} PRIVATE ${BITFOCUS_HDRS} ${BITFOCUS_SRCS})
  if(WIN32)
      target_sources(${PROJECT_NAME} PRIVATE  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusContext.win32.cpp")
  else()
      target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Bitfocus/BitfocusContext.unix.cpp")
  endif()
  target_compile_definitions(${PROJECT_NAME} PRIVATE OSSIA_PROTOCOL_BITFOCUS)
  list(APPEND SCORE_FEATURES_LIST protocol_bitfocus)
endif()

target_link_libraries(${PROJECT_NAME}
        PUBLIC
          ${QT_PREFIX}::Core ${QT_PREFIX}::Widgets ${QT_PREFIX}::Network
          score_lib_base score_lib_device score_plugin_deviceexplorer
          score_plugin_library
          ossia
        PRIVATE
          re2
)

if(OSSIA_DNSSD)
  target_link_libraries(${PROJECT_NAME} PRIVATE Servus_iface)
  target_include_directories(${PROJECT_NAME} PRIVATE ${OSSIA_3RDPARTY_FOLDER}/Servus)
endif()

if(TARGET "${QT_PREFIX}::Qml")
    target_sources(${PROJECT_NAME} PRIVATE ${MAPPER_SRCS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${QT_PREFIX}::Qml)
    list(APPEND SCORE_FEATURES_LIST protocol_mapper)
endif()

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

setup_score_plugin(${PROJECT_NAME})
