// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_fx.hpp"
#include <Fx/AngleNode.hpp>
#include <Fx/Quantifier.hpp>
#include <Fx/Chord.hpp>
#include <Fx/VelToNote.hpp>
#if !defined(INCOMPETENT_COMPILER)
#include <Fx/TestNode.hpp>
#include <Fx/LFO.hpp>
#include <Fx/Metro.hpp>
#include <Fx/Envelope.hpp>
#include <Fx/MathGenerator.hpp>
#include <Fx/MathMapping.hpp>
#include <Fx/EmptyMapping.hpp>
#include <Fx/MidiUtil.hpp>
#endif
#include <score/plugins/customfactory/FactorySetup.hpp>

#include <score_plugin_engine.hpp>

namespace Control
{

}
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
#if !defined(INCOMPETENT_COMPILER)
  return Control::instantiate_fx<
      Nodes::PulseToNote::Node
      Nodes::LFO::Node, 
      Nodes::Chord::Node, 
      Nodes::MidiUtil::Node,
      Nodes::Metro::Node,
      Nodes::Envelope::Node,
      Nodes::Quantifier::Node,
      Nodes::MathGenerator::Node,
      Nodes::MathAudioGenerator::Node,
      Nodes::MathMapping::Node,
      Nodes::EmptyValueMapping::Node,
      Nodes::EmptyMidiMapping::Node,
      Nodes::EmptyAudioMapping::Node
      >(ctx, key);
#else
  
  return Control::instantiate_fx<
      
      Nodes::PulseToNote::Node,
      Nodes::Direction::Node,
      Nodes::Chord::Node,
      Nodes::Quantifier::Node
      >(ctx, key);
  /*
  static_assert(Control::has_state_t<Nodes::Quantifier::Node>::value);
  //constexpr auto tutu = Nodes::Quantifier::Node::info;
  Control::ControlProcess<Nodes::Quantifier::Node>* n;
  new Control::ControlProcess<Nodes::Quantifier::Node>{TimeVal{}, Id<Process::ProcessModel>{}, nullptr};
  //Process::ProcessFactory_T<Control::ControlProcess<Nodes::Quantifier::Node>> factory;
  //Control::ControlLayerFactory<Nodes::Quantifier::Node> layer;
  //Control::InspectorFactory<Nodes::Quantifier::Node> inspector;
  //Control::ExecutorFactory<Nodes::Direction::Node> executor;
  //Control::ControlProcess<Nodes::Quantifier::Node>* n;
  Engine::Execution::Context* ctx2;
  //Control::Executor<Nodes::Direction::Node> exec(*n, *ctx2, Id<score::Component>{}, {});
  std::shared_ptr<Control::ControlNode<Nodes::Quantifier::Node>> ctl;
  
  
  Control::setup_node2<Nodes::Quantifier::Node>(ctl, *n, *ctx2, nullptr);
  return {};
  */
#endif
}

auto score_plugin_fx::required() const
-> std::vector<score::PluginKey>
{
  return { score_plugin_engine::static_key() };
}
