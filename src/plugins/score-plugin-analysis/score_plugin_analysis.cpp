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
  oscr::instantiate_fx<Analysis::Centroid>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::CSD>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::Crest>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::EnergyDifference>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::Flatness>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::HFQ>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::Kurtosis>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::MelSpectrum>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::MFCC>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::Peak>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::Pitch>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::RMS>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::Rolloff>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::SpectralDiff>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::SpectralDiffHWR>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::Spectrum>(fx, ctx, key);
  oscr::instantiate_fx<Analysis::ZeroCrossing>(fx, ctx, key);
  return fx;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_analysis)
