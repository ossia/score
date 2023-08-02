/*
#include <Examples/AudioEffect.hpp>
#include <Examples/AudioEffectWithSidechains.hpp>
#include <Examples/CCC.hpp>
#include <Examples/ControlGallery.hpp>
#include <Examples/Distortion.hpp>
#include <Examples/Empty.hpp>
#include <Examples/RawPorts.hpp>
#include <Examples/SampleAccurateFilter.hpp>
#include <Examples/SampleAccurateGenerator.hpp>
#include <Examples/Synth.hpp>
#include <Examples/TextureFilter.hpp>
#include <Examples/TextureGenerator.hpp>
#include <Examples/TrivialFilter.hpp>
#include <Examples/TrivialGenerator.hpp>
#include <Examples/ZeroDependencyAudioEffect.hpp>

*/
#include "score_plugin_avnd.hpp"

#include <score/plugins/FactorySetup.hpp>

#include <score_plugin_engine.hpp>

// clang-format off
#include <Avnd/Factories.hpp>
#include "include.avnd.cpp"
// clang-format on

namespace oscr
{
void instantiate_audiofilters(
    std::vector<score::InterfaceBase*>& fx, const score::ApplicationContext& ctx,
    const score::InterfaceKey& key);
void instantiate_aether(
    std::vector<score::InterfaceBase*>& fx, const score::ApplicationContext& ctx,
    const score::InterfaceKey& key);
void instantiate_convolver(
    std::vector<score::InterfaceBase*>& fx, const score::ApplicationContext& ctx,
    const score::InterfaceKey& key);
void instantiate_tests(
    std::vector<score::InterfaceBase*>& fx, const score::ApplicationContext& ctx,
    const score::InterfaceKey& key);
void instantiate_tests2(
    std::vector<score::InterfaceBase*>& fx, const score::ApplicationContext& ctx,
    const score::InterfaceKey& key);
}

score_plugin_avnd::score_plugin_avnd() = default;
score_plugin_avnd::~score_plugin_avnd() = default;

std::vector<score::InterfaceBase*> score_plugin_avnd::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  using namespace oscr;

  std::vector<score::InterfaceBase*> fx;

  // score_plugin_avnd.tests.cpp
  instantiate_tests(fx, ctx, key);
  instantiate_tests2(fx, ctx, key);

  // score_plugin_avnd.aether.cpp
  instantiate_aether(fx, ctx, key);

  // score_plugin_avnd.filters.cpp
  instantiate_audiofilters(fx, ctx, key);

  // score_plugin_avnd.convolver.cpp
  instantiate_convolver(fx, ctx, key);

  // cmake-generated .cpp's
  all_custom_factories(fx, ctx, key);

  return fx;
}

std::vector<score::PluginKey> score_plugin_avnd::required() const
{
  return {score_plugin_engine::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_avnd)
