project(score_plugin_protocols LANGUAGES CXX)

score_common_setup()

# Packages
find_package(Qt5 5.9 REQUIRED COMPONENTS Core Widgets Network)
find_package(Qt5 5.9 OPTIONAL_COMPONENTS Qml)

# Files & main target
set(HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/OSSIADevice.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/ProtocolLibrary.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/RateWidget.hpp"

    "${CMAKE_CURRENT_SOURCE_DIR}/score_plugin_protocols.hpp"
    )

set(SRCS

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
)
set(MIDI_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDIProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/MIDI/MIDISpecificSettingsSerialization.cpp"
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
)

set(ARTNET_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/ArtnetDevice.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/ArtnetProtocolFactory.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/ArtnetProtocolSettingsWidget.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Artnet/ArtnetSpecificSettingsSerialization.cpp"
)

set(MAPPER_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Mapper/MapperDevice.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/Protocols/Mapper/MapperDevice.cpp"
)

add_library(${PROJECT_NAME} ${SRCS} ${HDRS})

if(OSSIA_PROTOCOL_OSC)
    target_sources(${PROJECT_NAME} PRIVATE ${OSC_HDRS} ${OSC_SRCS})
endif()

if(OSSIA_PROTOCOL_MINUIT)
    target_sources(${PROJECT_NAME} PRIVATE ${MINUIT_HDRS} ${MINUIT_SRCS})
endif()

if(OSSIA_PROTOCOL_OSCQUERY)
  target_sources(${PROJECT_NAME} PRIVATE ${OSCQUERY_HDRS} ${OSCQUERY_SRCS})
endif()

if(OSSIA_PROTOCOL_MIDI)
    target_sources(${PROJECT_NAME} PRIVATE ${MIDI_HDRS} ${MIDI_SRCS})
endif()

if(OSSIA_PROTOCOL_HTTP)
    target_sources(${PROJECT_NAME} PRIVATE ${HTTP_HDRS} ${HTTP_SRCS})
endif()

if(OSSIA_PROTOCOL_WEBSOCKETS)
    target_sources(${PROJECT_NAME} PRIVATE ${WS_HDRS} ${WS_SRCS})
endif()

if(OSSIA_PROTOCOL_SERIAL)
    target_sources(${PROJECT_NAME} PRIVATE ${SERIAL_HDRS} ${SERIAL_SRCS})
    find_package(Qt5 5.7 REQUIRED COMPONENTS SerialPort)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::SerialPort)
endif()

if(OSSIA_PROTOCOL_PHIDGETS)
    target_sources(${PROJECT_NAME} PRIVATE ${PHIDGETS_HDRS} ${PHIDGETS_SRCS})
endif()

if(OSSIA_PROTOCOL_JOYSTICK)
    target_sources(${PROJECT_NAME} PRIVATE ${JOYSTICK_HDRS} ${JOYSTICK_SRCS})
endif()

if(OSSIA_PROTOCOL_WIIMOTE)
    target_sources(${PROJECT_NAME} PRIVATE ${WIIMOTE_HDRS} ${WIIMOTE_SRCS})
endif()


if(OSSIA_PROTOCOL_ARTNET)
    target_sources(${PROJECT_NAME} PRIVATE ${ARTNET_HDRS} ${ARTNET_SRCS})
endif()

target_link_libraries(${PROJECT_NAME}
        PUBLIC
          Qt5::Core Qt5::Widgets Qt5::Network
          score_lib_base score_lib_device score_plugin_deviceexplorer
          score_plugin_library
          ossia
)

if(TARGET Qt5::Qml)
    target_sources(${PROJECT_NAME} PRIVATE ${MAPPER_SRCS})
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Qml)
endif()

setup_score_plugin(${PROJECT_NAME})
