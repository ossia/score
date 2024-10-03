// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "score_plugin_analysis.hpp"

#include <score/plugins/FactorySetup.hpp>

#include <Analysis/Centroid.hpp>
#include <Analysis/ComplexSpectralDifference.hpp>
#include <Analysis/Crest.hpp>
#include <Analysis/EnergyDifference.hpp>
#include <Analysis/Envelope.hpp>
#include <Analysis/Flatness.hpp>
#include <Analysis/HighFrequencyContent.hpp>
#include <Analysis/Kurtosis.hpp>
#include <Analysis/MFCC.hpp>
#include <Analysis/Pitch.hpp>
#include <Analysis/Rolloff.hpp>
#include <Analysis/SpectralDifference.hpp>
#include <Analysis/SpectralDifference_HWR.hpp>
#include <Analysis/ZeroCrossing.hpp>
#include <Avnd/Factories.hpp>

score_plugin_analysis::score_plugin_analysis() = default;
score_plugin_analysis::~score_plugin_analysis() = default;

std::vector<score::InterfaceBase*> score_plugin_analysis::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  std::vector<score::InterfaceBase*> fx;
  oscr::instantiate_fx<A2::Centroid>(fx, ctx, key);
  oscr::instantiate_fx<A2::CSD>(fx, ctx, key);
  oscr::instantiate_fx<A2::Crest>(fx, ctx, key);
  oscr::instantiate_fx<A2::EnergyDifference>(fx, ctx, key);
  oscr::instantiate_fx<A2::Flatness>(fx, ctx, key);
  oscr::instantiate_fx<A2::HFQ>(fx, ctx, key);
  oscr::instantiate_fx<A2::Kurtosis>(fx, ctx, key);
  oscr::instantiate_fx<A2::MelSpectrum>(fx, ctx, key);
  oscr::instantiate_fx<A2::MFCC>(fx, ctx, key);
  oscr::instantiate_fx<A2::Peak>(fx, ctx, key);
  oscr::instantiate_fx<A2::Pitch>(fx, ctx, key);
  oscr::instantiate_fx<A2::RMS>(fx, ctx, key);
  oscr::instantiate_fx<A2::Rolloff>(fx, ctx, key);
  oscr::instantiate_fx<A2::SpectralDiff>(fx, ctx, key);
  oscr::instantiate_fx<A2::SpectralDiffHWR>(fx, ctx, key);
  oscr::instantiate_fx<A2::Spectrum>(fx, ctx, key);
  oscr::instantiate_fx<A2::ZeroCrossing>(fx, ctx, key);
  return fx;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_analysis)
