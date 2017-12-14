// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_fx.hpp"
#include <Fx/AngleNode.hpp>
#include <Fx/MidiUtil.hpp>
#include <Fx/TestNode.hpp>
#include <Fx/VelToNote.hpp>
#include <Fx/LFO.hpp>
#include <Fx/Metro.hpp>
#include <Fx/Envelope.hpp>
#include <Fx/Chord.hpp>
#include <Media/Effect/Generic/Effect.hpp>
#include <score/plugins/customfactory/FactorySetup.hpp>

#include <score_plugin_engine.hpp>


score_plugin_fx::score_plugin_fx() : QObject{}
{

}

score_plugin_fx::~score_plugin_fx()
{
}

std::vector<std::unique_ptr<score::InterfaceBase>>
score_plugin_fx::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
            score::ApplicationContext,
            FW<Engine::Execution::ProcessComponentFactory
               , Nodes::Direction::Factories::executor_factory
               , Nodes::PulseToNote::Factories::executor_factory
               , Nodes::LFO::Factories::executor_factory
               , Nodes::Chord::Factories::executor_factory
               , Nodes::MidiUtil::Factories::executor_factory
               , Nodes::Metro::Factories::executor_factory
               , Nodes::Envelope::Factories::executor_factory
            >,
      FW<Process::ProcessModelFactory
         , Nodes::Direction::Factories::process_factory
         , Nodes::PulseToNote::Factories::process_factory
         , Nodes::LFO::Factories::process_factory
         , Nodes::Chord::Factories::process_factory
         , Nodes::Metro::Factories::process_factory
         , Nodes::Envelope::Factories::process_factory
         , Nodes::MidiUtil::Factories::process_factory>,
      FW<Media::Effect::EffectFactory
         , Process::ControlEffectFactory<Nodes::Direction::Node>
         , Process::ControlEffectFactory<Nodes::PulseToNote::Node>
         , Process::ControlEffectFactory<Nodes::LFO::Node>
         , Process::ControlEffectFactory<Nodes::Chord::Node>
         , Process::ControlEffectFactory<Nodes::Metro::Node>
         , Process::ControlEffectFactory<Nodes::Envelope::Node>
         , Process::ControlEffectFactory<Nodes::MidiUtil::Node>>
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
         , Nodes::MidiUtil::Factories::layer_factory>,
      FW<Engine::Execution::EffectComponentFactory
      , Engine::Execution::ControlEffectComponentFactory<Nodes::Direction::Node>
      , Engine::Execution::ControlEffectComponentFactory<Nodes::PulseToNote::Node>
      , Engine::Execution::ControlEffectComponentFactory<Nodes::LFO::Node>
      , Engine::Execution::ControlEffectComponentFactory<Nodes::Chord::Node>
      , Engine::Execution::ControlEffectComponentFactory<Nodes::Metro::Node>
      , Engine::Execution::ControlEffectComponentFactory<Nodes::Envelope::Node>
      , Engine::Execution::ControlEffectComponentFactory<Nodes::MidiUtil::Node>>
    >(ctx, key);

}

auto score_plugin_fx::required() const
  -> std::vector<score::PluginKey>
{
    return { score_plugin_engine::static_key() };
}
