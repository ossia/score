// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "score_plugin_engine.hpp"

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Process/Execution/ProcessComponent.hpp>
#include <Protocols/Local/LocalProtocolFactory.hpp>

#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/FactorySetup.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>

#include <ossia/editor/scenario/time_event.hpp>
#include <ossia/network/base/device.hpp>

#include <QString>

#include <Audio/AudioPanel.hpp>
#include <Audio/DummyInterface.hpp>
#include <Audio/JackInterface.hpp>
#include <Audio/PortAudioInterface.hpp>
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
#  include <Protocols/Wiimote/WiimoteProtocolFactory.hpp>
#endif

#include <Protocols/Audio/AudioDevice.hpp>

#include <Execution/Dataflow/DataflowClock.hpp>
#include <Execution/Dataflow/ManualClock.hpp>
#include <score_plugin_deviceexplorer.hpp>
#include <score_plugin_scenario.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Execution::ManualClock::TimeWidget)

score_plugin_engine::score_plugin_engine()
{
  qRegisterMetaType<Execution::ClockFactory::ConcreteKey>("ClockKey");
  qRegisterMetaTypeStreamOperators<Execution::ClockFactory::ConcreteKey>(
      "ClockKey");

  qRegisterMetaType<Audio::AudioFactory::ConcreteKey>("AudioKey");
  qRegisterMetaTypeStreamOperators<Audio::AudioFactory::ConcreteKey>(
      "AudioKey");
}

score_plugin_engine::~score_plugin_engine()
{
}

score::GUIApplicationPlugin* score_plugin_engine::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  return new Engine::ApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_engine::factoryFamilies()
{
  return make_ptr_vector<
      score::InterfaceListBase, LocalTree::ProcessComponentFactoryList,
      Execution::ProcessComponentFactoryList, Execution::ClockFactoryList,
      Audio::AudioFactoryList>();
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_engine::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Engine;
  using namespace Execution;
  return instantiate_factories<
      score::ApplicationContext,
      FW<Device::ProtocolFactory, Network::LocalProtocolFactory

#if defined(OSSIA_PROTOCOL_OSC)
         ,
         Network::OSCProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_MINUIT)
         ,
         Network::MinuitProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_OSCQUERY)
         ,
         Network::OSCQueryProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_MIDI)
         ,
         Network::MIDIProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_HTTP)
         ,
         Network::HTTPProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_WEBSOCKETS)
         ,
         Network::WSProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_SERIAL)
         ,
         Network::SerialProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_PHIDGETS)
         ,
         Network::PhidgetProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_AUDIO)
         ,
         Dataflow::AudioProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_JOYSTICK)
         ,
         Network::JoystickProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_WIIMOTE)
        ,
        Network::WiimoteProtocolFactory
#endif
         >,

      FW<Audio::AudioFactory, Audio::DummyFactory
#if defined(OSSIA_AUDIO_JACK)
         ,
         Audio::JackFactory
#endif
#if defined(OSSIA_AUDIO_PORTAUDIO)
         ,
         Audio::PortAudioFactory
#endif
#if defined(OSSIA_AUDIO_SDL)
         ,
         Audio::SDLFactory
#endif
         >,

      FW<Explorer::ListeningHandlerFactory,
         Execution::PlayListeningHandlerFactory>,
      FW<score::SettingsDelegateFactory, Execution::Settings::Factory,
         Audio::Settings::Factory>,
      FW<LocalTree::ProcessComponentFactory,
         LocalTree::ScenarioComponentFactory, LocalTree::LoopComponentFactory,
         LocalTree::AutomationComponentFactory,
         LocalTree::MappingComponentFactory>,
      FW<score::PanelDelegateFactory, Audio::PanelDelegateFactory>,
      FW<Execution::ClockFactory
         // , Execution::ControlClockFactory
         ,
         Dataflow::ClockFactory
         // , Engine::ManualClock::ClockFactory
         >>(ctx, key);
}

auto score_plugin_engine::required() const -> std::vector<score::PluginKey>
{
  return {score_plugin_scenario::static_key(),
          score_plugin_deviceexplorer::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_engine)
