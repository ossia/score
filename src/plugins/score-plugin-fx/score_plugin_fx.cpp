// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#if defined(_MSC_VER)
#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#if !defined(NOMINMAX)
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "score_plugin_fx.hpp"

#include <Fx/AngleNode.hpp>
#include <Fx/Arpeggiator.hpp>
#include <Fx/Arraygen.hpp>
#include <Fx/Arraymap.hpp>
#include <Fx/Chord.hpp>
#include <Fx/ClassicalBeat.hpp>
#include <Fx/EmptyMapping.hpp>
#include <Fx/Envelope.hpp>
#include <Fx/LFO.hpp>
#include <Fx/LFO_v2.hpp>
#include <Fx/Looper.hpp>
#include <Fx/MathAudioFilter.hpp>
#include <Fx/MathAudioGenerator.hpp>
#include <Fx/MathGenerator.hpp>
#include <Fx/MathValueFilter.hpp>
#include <Fx/Metro.hpp>
#include <Fx/MicroMapping.hpp>
#include <Fx/MidiHiRes.hpp>
#include <Fx/MidiToArray.hpp>
#include <Fx/MidiUtil.hpp>
#include <Fx/PitchToValue.hpp>
#include <Fx/Quantifier.hpp>
#include <Fx/RateLimiter.hpp>
#include <Fx/Smooth.hpp>
#include <Fx/Smooth_v2.hpp>
#include <Fx/VelToNote.hpp>
#include <Fx/VelToNote_mono.hpp>
/*
#include <Fx/FactorOracle.hpp>
#include <Fx/FactorOracle2.hpp>
#include <Fx/FactorOracle2MIDI.hpp>
#include <Fx/MathMapping.hpp>
#include <Fx/TestNode.hpp>
#if defined(SCORE_DEBUG)
#include <Fx/DebugFx.hpp>
#endif

*/

#include <score/plugins/FactorySetup.hpp>

#include <Avnd/Factories.hpp>

#include <score_plugin_engine.hpp>
score_plugin_fx::score_plugin_fx() = default;
score_plugin_fx::~score_plugin_fx() = default;

std::vector<score::InterfaceBase*> score_plugin_fx::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  std::vector<score::InterfaceBase*> fx;
  oscr::instantiate_fx<Nodes::Direction>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::Arpeggiator::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::Chord::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::ClassicalBeat::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::EmptyValueMapping::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::EmptyMidiMapping::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::EmptyAudioMapping::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::Envelope::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::PulseToNote::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::PulseToNoteMono::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::LFO::v1::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::LFO::v2::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::MidiHiRes::Input>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::MidiHiRes::Output>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::MidiUtil::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::MidiToArray::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::Metro::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::Quantifier::Node>(fx, ctx, key);

  oscr::instantiate_fx<Nodes::MathGenerator::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::MathAudioGenerator::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::MathMapping::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::MicroMapping::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::ArrayGenerator::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::ArrayMapping::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::MathAudioFilter::Node>(fx, ctx, key);

  //oscr::instantiate_fx<Nodes::FactorOracle::Node>(fx, ctx, key);
  //oscr::instantiate_fx<Nodes::FactorOracle2::Node>(fx, ctx, key);
  //oscr::instantiate_fx<Nodes::FactorOracle2MIDI::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::PitchToValue::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::Smooth::v1::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::Smooth::v2::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::RateLimiter::Node>(fx, ctx, key);
  oscr::instantiate_fx<Nodes::AudioLooper::Node>(fx, ctx, key);

#if defined(SCORE_DEBUG)
  //oscr::instantiate_fx<Nodes::Debug::Node>(fx, ctx, key);
#endif
  return fx;
}

auto score_plugin_fx::required() const -> std::vector<score::PluginKey>
{
  return {score_plugin_engine::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_fx)
