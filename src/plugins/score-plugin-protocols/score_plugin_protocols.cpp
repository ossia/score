// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "score_plugin_protocols.hpp"

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Protocols/ProtocolLibrary.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>

#include <ossia-config.hpp>
#if defined(OSSIA_PROTOCOL_MINUIT)
#include <Protocols/Minuit/MinuitProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_OSC)
#include <Protocols/OSC/OSCProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_OSCQUERY)
#include <Protocols/OSCQuery/OSCQueryProtocolFactory.hpp>
#endif

#if defined(OSSIA_PROTOCOL_MIDI)
#include <Protocols/MIDI/MIDIProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_HTTP)
#include <Protocols/HTTP/HTTPProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_WEBSOCKETS)
#include <Protocols/WS/WSProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_SERIAL)
#include <Protocols/Serial/SerialProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_PHIDGETS)
#include <Protocols/Phidgets/PhidgetsProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_JOYSTICK)
#include <Protocols/Joystick/JoystickProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_WIIMOTE)
#include <Protocols/Wiimote/WiimoteProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_ARTNET)
#include <Protocols/Artnet/ArtnetProtocolFactory.hpp>
#endif

#include <Protocols/Mapper/MapperDevice.hpp>

#include <score_plugin_deviceexplorer.hpp>
#include <wobjectimpl.h>
score_plugin_protocols::score_plugin_protocols()
{
#if __has_include(<QQmlEngine>)
  qmlRegisterType<Protocols::Mapper>("Ossia", 1, 0, "Mapper");
#endif
  qRegisterMetaType<std::vector<ossia::net::node_base*>>("std::vector<ossia::net::node_base*>");
}

score_plugin_protocols::~score_plugin_protocols() { }

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_protocols::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Device::ProtocolFactory
#if __has_include(<QQmlEngine>)
         ,
         Protocols::MapperProtocolFactory
#endif

#if defined(OSSIA_PROTOCOL_OSC)
         ,
         Protocols::OSCProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_MINUIT)
         ,
         Protocols::MinuitProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_OSCQUERY)
         ,
         Protocols::OSCQueryProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_MIDI)
         ,
         Protocols::MIDIProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_HTTP)
         ,
         Protocols::HTTPProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_WEBSOCKETS)
         ,
         Protocols::WSProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_SERIAL)
         ,
         Protocols::SerialProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_PHIDGETS)
         ,
         Protocols::PhidgetProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_JOYSTICK)
         ,
         Protocols::JoystickProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_WIIMOTE)
         ,
         Protocols::WiimoteProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_ARTNET)
         ,
         Protocols::ArtnetProtocolFactory
#endif
         >,
      FW<Library::LibraryInterface,
         Protocols::OSCLibraryHandler
#if __has_include(<QQmlEngine>)
         ,
         Protocols::QMLLibraryHandler
#endif
         >>(ctx, key);
}

auto score_plugin_protocols::required() const -> std::vector<score::PluginKey>
{
  return {score_plugin_deviceexplorer::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_protocols)
