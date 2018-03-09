// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_fx.hpp"
#if !defined(INCOMPETENT_COMPILER)
#include <Fx/Quantifier.hpp>
#include <Fx/AngleNode.hpp>
#include <Fx/TestNode.hpp>
#include <Fx/VelToNote.hpp>
#include <Fx/LFO.hpp>
#include <Fx/Metro.hpp>
#include <Fx/Envelope.hpp>
#include <Fx/Chord.hpp>
#include <Fx/MathGenerator.hpp>
#include <Fx/MathMapping.hpp>
#include <Fx/EmptyMapping.hpp>
#include <Fx/MidiUtil.hpp>
#endif
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
#if !defined(INCOMPETENT_COMPILER)
  return Control::instantiate_fx<
      Nodes::Direction::Node,
      Nodes::PulseToNote::Node,
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
  return {};
#endif
}

auto score_plugin_fx::required() const
  -> std::vector<score::PluginKey>
{
    return { score_plugin_engine::static_key() };
}
