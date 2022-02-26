
#include <Examples/AudioEffect.hpp>
#include <Examples/AudioEffectWithSidechains.hpp>
#include <Examples/Empty.hpp>
#include <Examples/Distortion.hpp>
#include <Examples/ZeroDependencyAudioEffect.hpp>

#include <Examples/SampleAccurateGenerator.hpp>
#include <Examples/SampleAccurateFilter.hpp>

#include <Examples/TextureGenerator.hpp>
#include <Examples/TextureFilter.hpp>
#include <Examples/TrivialGenerator.hpp>
#include <Examples/TrivialFilter.hpp>
#include <Examples/RawPorts.hpp>
#include <Examples/ControlGallery.hpp>

#include <Examples/CCC.hpp>
#include <Examples/Synth.hpp>


#include "score_plugin_avnd.hpp"

#include <Crousti/ProcessModel.hpp>
#include <Crousti/Executor.hpp>
#include <Crousti/Layer.hpp>

#include <score/plugins/FactorySetup.hpp>

#include <boost/pfr.hpp>
#include <Process/ProcessMetadata.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Dataflow/PortFactory.hpp>
#include <score_plugin_engine.hpp>
#include <ossia/detail/typelist.hpp>

/**
 * This file instantiates the classes that are provided by this plug-in.
 */



namespace oscr
{
template <typename Node>
using ProcessFactory = Process::ProcessFactory_T<oscr::ProcessModel<Node>>;
/*

template <oscr::DataflowNode Node>
struct ExecutorFactory final
    : public Execution::ProcessComponentFactory_T<oscr::Executor<Node>>
{
  using Execution::ProcessComponentFactory_T<
      oscr::Executor<Node>>::ProcessComponentFactory_T;
};
*/
 //template <typename Node>
 //using LayerFactory = oscr::LayerFactory<Node>;


template <typename... Nodes>
std::vector<std::unique_ptr<score::InterfaceBase>> instantiate_fx(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key)
{
  std::vector<std::unique_ptr<score::InterfaceBase>> v;
  if (key == Execution::ProcessComponentFactory::static_interfaceKey())
  {
    //static_assert((requires { std::declval<Nodes>().run({}, {}); } && ...));
    (v.push_back(std::make_unique<oscr::ExecutorFactory<Nodes>>()), ...);
  }
  else
    if (key == Process::ProcessModelFactory::static_interfaceKey())
  {
    (v.emplace_back(new oscr::ProcessFactory<Nodes>()), ...);
  }



  //else if (key == Process::LayerFactory::static_interfaceKey())
  //{
  //  (v.push_back(make_interface<oscr::LayerFactory<Nodes>>()), ...);
  //}
  return v;
}
}

score_addon_simpleapi2::score_addon_simpleapi2() = default;
score_addon_simpleapi2::~score_addon_simpleapi2() = default;

std::vector<std::unique_ptr<score::InterfaceBase>>
score_addon_simpleapi2::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  using namespace oscr;
  using namespace examples;
  return oscr::instantiate_fx</*
      Distortion
      , AudioEffectExample
      , AudioSidechainExample
      , EmptyExample
      , SampleAccurateGeneratorExample
      , SampleAccurateFilterExample
      , ControlGallery
      , CCC
      , RawPortsExample
      , Synth
#if SCORE_PLUGIN_GFX
      , TextureGeneratorExample
      , TextureFilterExample
#endif
      , TrivialGeneratorExample
      , TrivialFilterExample
      , */ZeroDependencyAudioEffect>(ctx, key);
  return {};
}

std::vector<score::PluginKey> score_addon_simpleapi2::required() const
{
  return {score_plugin_engine::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_addon_simpleapi2)
