// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Engine/Protocols/Local/LocalProtocolFactory.hpp>
#include <Engine/Protocols/Minuit/MinuitProtocolFactory.hpp>
#include <Engine/Protocols/OSC/OSCProtocolFactory.hpp>
#include <Engine/Protocols/OSCQuery/OSCQueryProtocolFactory.hpp>
#include <Engine/Protocols/Serial/SerialProtocolFactory.hpp>

#include <Engine/Protocols/Panel/MessagesPanel.hpp>

#include <QString>

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Engine/ApplicationPlugin.hpp>

#include "score_plugin_engine.hpp"
#include <Engine/Executor/ProcessComponent.hpp>
#include <Engine/Executor/ScenarioComponent.hpp>
#include <Engine/LocalTree/Scenario/LoopComponent.hpp>
#include <Engine/LocalTree/Scenario/AutomationComponent.hpp>
#include <Engine/LocalTree/Scenario/MappingComponent.hpp>
#include <Engine/LocalTree/Scenario/ScenarioComponent.hpp>
#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>

#include <Engine/Curve/EasingSegment.hpp>
#include <Engine/Executor/ClockManager/ClockManagerFactory.hpp>
#include <Engine/Executor/ClockManager/DefaultClockManager.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/Executor/Interpolation/InterpolationComponent.hpp>
#include <Engine/Executor/Mapping/Component.hpp>
#include <Engine/Executor/Loop/Component.hpp>
#include <Engine/Executor/Automation/Component.hpp>
#include <Engine/Executor/Automation/GradientComponent.hpp>
#include <Engine/Executor/Automation/SplineComponent.hpp>
#include <Engine/Executor/Automation/MetronomeComponent.hpp>
#include <Engine/Executor/Settings/ExecutorFactory.hpp>
#include <Engine/Executor/StateProcessComponent.hpp>
#include <Engine/Listening/PlayListeningHandlerFactory.hpp>
#include <Engine/LocalTree/Settings/LocalTreeFactory.hpp>
#include <score/plugins/customfactory/FactorySetup.hpp>

#include <Engine/Node/Nodes/MidiUtil.hpp>
#include <Engine/Node/Nodes/TestNode.hpp>
#include <Engine/Node/Nodes/AngleNode.hpp>
#include <Engine/Node/Nodes/VelToNote.hpp>
#include <Engine/Node/Nodes/LFO.hpp>
#include <Engine/Node/Nodes/Metro.hpp>
#include <Engine/Node/Nodes/Envelope.hpp>
#include <Engine/Node/Nodes/Chord.hpp>

#if defined(OSSIA_PROTOCOL_MIDI)
#include <Engine/Protocols/MIDI/MIDIProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_HTTP)
#include <Engine/Protocols/HTTP/HTTPProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_WEBSOCKETS)
#include <Engine/Protocols/WS/WSProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_SERIAL)
#include <Engine/Protocols/Serial/SerialProtocolFactory.hpp>
#endif
#if defined(OSSIA_PROTOCOL_PHIDGETS)
#include <Engine/Protocols/Phidgets/PhidgetsProtocolFactory.hpp>
#endif
#if defined(OSSIA_DATAFLOW)
#include <Engine/Protocols/Audio/AudioDevice.hpp>
#include <Engine/Executor/Dataflow/DataflowClock.hpp>
#endif

#include <Scenario/score_plugin_scenario.hpp>
#include <score_plugin_deviceexplorer.hpp>

score_plugin_engine::score_plugin_engine() : QObject{}
{
  qRegisterMetaType<Engine::Execution::ClockManagerFactory::
                        ConcreteKey>("ClockManagerKey");
  qRegisterMetaTypeStreamOperators<Engine::Execution::ClockManagerFactory::
                                       ConcreteKey>("ClockManagerKey");


}

score_plugin_engine::~score_plugin_engine()
{
}

score::GUIApplicationPlugin*
score_plugin_engine::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  return new Engine::ApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_engine::factoryFamilies()
{
  return make_ptr_vector<score::InterfaceListBase, Engine::LocalTree::ProcessComponentFactoryList, Engine::Execution::ProcessComponentFactoryList, Engine::Execution::StateProcessComponentFactoryList, Engine::Execution::ClockManagerFactoryList>();
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_engine::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  using namespace Scenario;
  using namespace Engine;
  using namespace Engine::Execution;
  using namespace EasingCurve;

  return instantiate_factories<
            score::ApplicationContext,
            FW<Device::ProtocolFactory,
                 Network::LocalProtocolFactory,
                 Network::OSCProtocolFactory,
                 Network::MinuitProtocolFactory,
                 Network::OSCQueryProtocolFactory
#if defined(OSSIA_PROTOCOL_MIDI)
                 , Network::MIDIProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_HTTP)
                 , Network::HTTPProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_WEBSOCKETS)
                 , Network::WSProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_SERIAL)
                 , Network::SerialProtocolFactory
#endif
#if defined(OSSIA_PROTOCOL_PHIDGETS)
                 , Network::PhidgetProtocolFactory
#endif
#if defined(OSSIA_DATAFLOW)
                 , Dataflow::AudioProtocolFactory
#endif
            >,
            FW<Engine::Execution::ProcessComponentFactory
               , Engine::Execution::ScenarioComponentFactory
             //, Interpolation::Executor::ComponentFactory
               , Automation::RecreateOnPlay::ComponentFactory
               , Mapping::RecreateOnPlay::ComponentFactory
               , Loop::RecreateOnPlay::ComponentFactory
               , Nodes::Direction::Factories::executor_factory
               , Nodes::PulseToNote::Factories::executor_factory
               , Nodes::LFO::Factories::executor_factory
               , Nodes::Chord::Factories::executor_factory
               , Nodes::MidiUtil::Factories::executor_factory
               , Nodes::Metro::Factories::executor_factory
               , Nodes::Envelope::Factories::executor_factory
               , Gradient::RecreateOnPlay::ComponentFactory
                 //Spline::RecreateOnPlay::ComponentFactory,
                 //Metronome::RecreateOnPlay::ComponentFactory
            >,
            FW<Explorer::ListeningHandlerFactory,
                 Engine::Execution::PlayListeningHandlerFactory>,
            FW<score::SettingsDelegateFactory,
                 Engine::Execution::Settings::Factory,
                 LocalTree::Settings::Factory>,
            FW<Engine::LocalTree::ProcessComponentFactory,
                 Engine::LocalTree::ScenarioComponentFactory,
                 Engine::LocalTree::LoopComponentFactory,
                 Engine::LocalTree::AutomationComponentFactory,
                 Engine::LocalTree::MappingComponentFactory
            >,
            FW<score::PanelDelegateFactory, Engine::MessagesPanelDelegateFactory>,
            FW<Engine::Execution::ClockManagerFactory
              , Engine::Execution::ControlClockFactory
              , Dataflow::ClockFactory
            >,
            FW<Curve::SegmentFactory,
                Curve::SegmentFactory_T<Segment_backIn>,
                Curve::SegmentFactory_T<Segment_backOut>,
                Curve::SegmentFactory_T<Segment_backInOut>,
            Curve::SegmentFactory_T<Segment_bounceIn>,
            Curve::SegmentFactory_T<Segment_bounceOut>,
            Curve::SegmentFactory_T<Segment_bounceInOut>,
            Curve::SegmentFactory_T<Segment_quadraticIn>,
            Curve::SegmentFactory_T<Segment_quadraticOut>,
            Curve::SegmentFactory_T<Segment_quadraticInOut>,
            Curve::SegmentFactory_T<Segment_cubicIn>,
            Curve::SegmentFactory_T<Segment_cubicOut>,
            Curve::SegmentFactory_T<Segment_cubicInOut>,
            Curve::SegmentFactory_T<Segment_quarticIn>,
            Curve::SegmentFactory_T<Segment_quarticOut>,
            Curve::SegmentFactory_T<Segment_quarticInOut>,
            Curve::SegmentFactory_T<Segment_quinticIn>,
            Curve::SegmentFactory_T<Segment_quinticOut>,
            Curve::SegmentFactory_T<Segment_quinticInOut>,
            Curve::SegmentFactory_T<Segment_sineIn>,
            Curve::SegmentFactory_T<Segment_sineOut>,
            Curve::SegmentFactory_T<Segment_sineInOut>,
            Curve::SegmentFactory_T<Segment_circularIn>,
            Curve::SegmentFactory_T<Segment_circularOut>,
            Curve::SegmentFactory_T<Segment_circularInOut>,
            Curve::SegmentFactory_T<Segment_exponentialIn>,
            Curve::SegmentFactory_T<Segment_exponentialOut>,
            Curve::SegmentFactory_T<Segment_exponentialInOut>,
            Curve::SegmentFactory_T<Segment_elasticIn>,
            Curve::SegmentFactory_T<Segment_elasticOut>,
            Curve::SegmentFactory_T<Segment_elasticInOut>,
            Curve::SegmentFactory_T<Segment_perlinInOut>
            >,
      FW<Process::ProcessModelFactory
         , Nodes::Direction::Factories::process_factory
         , Nodes::PulseToNote::Factories::process_factory
         , Nodes::LFO::Factories::process_factory
      , Nodes::Chord::Factories::process_factory
      , Nodes::Metro::Factories::process_factory
      , Nodes::Envelope::Factories::process_factory
         , Nodes::MidiUtil::Factories::process_factory>
     , FW<Process::InspectorWidgetDelegateFactory
         , Nodes::Direction::Factories::inspector_factory
         , Nodes::PulseToNote::Factories::inspector_factory
         , Nodes::LFO::Factories::inspector_factory
      , Nodes::Chord::Factories::inspector_factory
      , Nodes::Metro::Factories::inspector_factory
      , Nodes::Envelope::Factories::inspector_factory
         , Nodes::MidiUtil::Factories::inspector_factory>
     , FW<Process::LayerFactory
         , Nodes::Direction::Factories::layer_factory
         , Nodes::PulseToNote::Factories::layer_factory
         , Nodes::LFO::Factories::layer_factory
      , Nodes::Chord::Factories::layer_factory
      , Nodes::Metro::Factories::layer_factory
      , Nodes::Envelope::Factories::layer_factory
         , Nodes::MidiUtil::Factories::layer_factory>
               >(ctx, key);
}

auto score_plugin_engine::required() const
  -> std::vector<score::PluginKey>
{
    return {
      score_plugin_scenario::static_key(),
      score_plugin_deviceexplorer::static_key()
    };
}
