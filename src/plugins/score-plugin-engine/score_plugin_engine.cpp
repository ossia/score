// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "score_plugin_engine.hpp"

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Protocols/Local/LocalProtocolFactory.hpp>
#include <Protocols/ProtocolLibrary.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>

#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/network/base/device.hpp>


#include <Audio/AudioPanel.hpp>
#include <Audio/DummyInterface.hpp>
#include <Audio/JackInterface.hpp>
#include <Audio/PortAudioInterface.hpp>
#include <Audio/ALSAPortAudioInterface.hpp>
#include <Audio/ASIOPortAudioInterface.hpp>
#include <Audio/CoreAudioPortAudioInterface.hpp>
#include <Audio/GenericPortAudioInterface.hpp>
#include <Audio/MMEPortAudioInterface.hpp>
#include <Audio/WASAPIPortAudioInterface.hpp>
#include <Audio/WDMKSPortAudioInterface.hpp>
#include <Audio/SDLInterface.hpp>
#include <Audio/Settings/Factory.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <Engine/Listening/PlayListeningHandlerFactory.hpp>
#include <Execution/Clock/ClockFactory.hpp>
#include <Execution/Clock/DefaultClock.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/Settings/ExecutorFactory.hpp>
#include <LocalTree/Scenario/AutomationComponent.hpp>
#include <LocalTree/Scenario/LoopComponent.hpp>
#include <LocalTree/Scenario/MappingComponent.hpp>
#include <LocalTree/Scenario/ScenarioComponent.hpp>

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

#include <Protocols/Audio/AudioDevice.hpp>
#include <Protocols/Mapper/MapperDevice.hpp>

#include <Execution/Clock/DataflowClock.hpp>
#include <Execution/Clock/ManualClock.hpp>
#include <score_plugin_deviceexplorer.hpp>
#include <score_plugin_scenario.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Execution::ManualClock::TimeWidget)

score_plugin_engine::score_plugin_engine()
{
#if __has_include(<QQmlEngine>)
  qmlRegisterType<Protocols::Mapper>("Ossia", 1, 0, "Mapper");
#endif
  qRegisterMetaType<Execution::ClockFactory::ConcreteKey>("ClockKey");
  qRegisterMetaTypeStreamOperators<Execution::ClockFactory::ConcreteKey>(
      "ClockKey");

  qRegisterMetaType<Audio::AudioFactory::ConcreteKey>("AudioKey");
  qRegisterMetaTypeStreamOperators<Audio::AudioFactory::ConcreteKey>(
      "AudioKey");
  qRegisterMetaType<std::vector<ossia::net::node_base*>>(
      "std::vector<ossia::net::node_base*>");
}

score_plugin_engine::~score_plugin_engine() {}

score::GUIApplicationPlugin* score_plugin_engine::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  return new Engine::ApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_engine::factoryFamilies()
{
  return make_ptr_vector<
      score::InterfaceListBase,
      LocalTree::ProcessComponentFactoryList,
      Execution::ProcessComponentFactoryList,
      Execution::ClockFactoryList,
      Audio::AudioFactoryList>();
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_engine::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Engine;
  using namespace Execution;
  return instantiate_factories<
      score::ApplicationContext,
      FW<Device::ProtocolFactory,
         Protocols::LocalProtocolFactory
    #if __has_include(<QQmlEngine>)
      , Protocols::MapperProtocolFactory
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
#if defined(OSSIA_PROTOCOL_AUDIO)
         ,
         Dataflow::AudioProtocolFactory
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

      FW<Audio::AudioFactory,
         Audio::DummyFactory
#if defined(OSSIA_AUDIO_JACK)
         ,
         Audio::JackFactory
#endif
#if defined(OSSIA_AUDIO_PORTAUDIO)
#if __has_include(<pa_asio.h>)
         ,
         Audio::ASIOFactory
#endif
#if __has_include(<pa_win_wdmks.h>)
         ,
         Audio::WDMKSFactory
#endif
#if __has_include(<pa_win_wasapi.h>)
         ,
         Audio::WASAPIFactory
#endif
#if __has_include(<pa_win_wmme.h>)
         ,
         Audio::MMEFactory
#endif
#if __has_include(<pa_linux_alsa.h>)
         ,
         Audio::ALSAFactory
#endif
#if __has_include(<pa_mac_core.h>)
         ,
         Audio::CoreAudioFactory
#endif
#if !__has_include(<pa_asio.h>) && \
    !__has_include(<pa_win_wdmks.h>) && \
    !__has_include(<pa_win_wasapi.h>) && \
    !__has_include(<pa_win_wmme.h>) && \
    !__has_include(<pa_linux_alsa.h>) && \
    !__has_include(<pa_mac_core.h>)
         ,
         Audio::PortAudioFactory
#endif
#endif
#if defined(OSSIA_AUDIO_SDL)
         ,
         Audio::SDLFactory
#endif
         >,

      FW<Explorer::ListeningHandlerFactory,
         Execution::PlayListeningHandlerFactory>,
      FW<score::SettingsDelegateFactory,
         Execution::Settings::Factory,
         Audio::Settings::Factory>,
      FW<LocalTree::ProcessComponentFactory,
         LocalTree::ScenarioComponentFactory,
         LocalTree::LoopComponentFactory,
         LocalTree::AutomationComponentFactory,
         LocalTree::MappingComponentFactory>,
      FW<score::PanelDelegateFactory, Audio::PanelDelegateFactory>,
      FW<Library::LibraryInterface,
         Protocols::OSCLibraryHandler
#if __has_include(<QQmlEngine>)
       , Protocols::QMLLibraryHandler
#endif
      >,
      FW<Execution::ClockFactory
         // , Execution::ControlClockFactory
         ,
         Dataflow::ClockFactory
         , ManualClock::ClockFactory
         >>(ctx, key);
}

auto score_plugin_engine::required() const -> std::vector<score::PluginKey>
{
  return {score_plugin_scenario::static_key(),
          score_plugin_deviceexplorer::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_engine)
