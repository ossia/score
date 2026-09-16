#include "score_plugin_gris.hpp"

#include <Gris/Commands.hpp>
#include <Gris/Executor.hpp>
#include <Gris/Inspector.hpp>
#include <Gris/Model.hpp>
#include <Gris/SpeakerSetupInlet.hpp>

#include <Process/Dataflow/PortFactory.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>
#include <Process/GenericProcessFactory.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>

#include <score/plugins/FactorySetup.hpp>

#include <score_plugin_engine.hpp>
#include <score_plugin_gris_commands_files.hpp>

score_plugin_gris::score_plugin_gris() = default;
score_plugin_gris::~score_plugin_gris() = default;

std::vector<score::InterfaceBase*> score_plugin_gris::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Process::ProcessFactory_T<Gris::SpatModel>>,
      FW<Process::LayerFactory, Process::EffectLayerFactory_T<Gris::SpatModel>>,
      FW<Process::PortFactory,
         Dataflow::WidgetInletFactory<
             Gris::SpeakerSetupInlet, WidgetFactory::SpeakerSetupWidget>>,
      FW<Execution::ProcessComponentFactory, Gris::ExecutorFactory>,
      FW<Inspector::InspectorWidgetFactory, Gris::InspectorFactory>>(ctx, key);
}

std::vector<score::PluginKey> score_plugin_gris::required() const
{
  return {score_plugin_engine::static_key()};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_gris)
