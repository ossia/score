#include "score_plugin_pd.hpp"

// We do this to pevent picking up a potential system /usr/inclde/m_pd.h
#include <libpd/pure-data/src/m_pd.h>
#include <z_libpd.h>

#include <score/plugins/FactorySetup.hpp>
#include <QDebug>
#include <Pd/Commands/PdCommandFactory.hpp>
#include <Pd/Executor/PdExecutor.hpp>
#include <Pd/PdFactory.hpp>
#include <Pd/PdLayer.hpp>
#include <Pd/PdLibrary.hpp>
#include <score_plugin_pd_commands_files.hpp>
#include <score_plugin_deviceexplorer.hpp>
#include <score_plugin_scenario.hpp>
std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_pd::make_commands()
{
  using namespace Pd;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      Pd::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_pd_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}
std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_pd::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, Pd::ProcessFactory>,
      FW<Process::InspectorWidgetDelegateFactory, Pd::InspectorFactory>,
      FW<Process::LayerFactory, Pd::LayerFactory>,
      FW<Library::LibraryInterface, Pd::LibraryHandler>,
      FW<Process::ProcessDropHandler, Pd::DropHandler>,
      FW<Execution::ProcessComponentFactory, Pd::ComponentFactory>>(ctx, key);
}

score_plugin_pd::score_plugin_pd()
{
  libpd_init();
  // TODO should not be necessarey
  libpd_init_audio(2, 2, 48000);

  /*
  libpd_set_printhook([](const char* s) { qDebug() << "string: " << s; });
  libpd_set_floathook(
      [](const char* s, float x) { qDebug() << "float: " << s << x; });
  */
}

auto score_plugin_pd::required() const -> std::vector<score::PluginKey>
{
  return {score_plugin_scenario::static_key(),
          score_plugin_deviceexplorer::static_key()};
}

score_plugin_pd::~score_plugin_pd() {}

score::Version score_plugin_pd::version() const
{
  return score::Version{1};
}

UuidKey<score::Plugin> score_plugin_pd::key() const
{
  return_uuid("ed87a509-7319-4303-8cf7-3bba849458cf");
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_pd)
