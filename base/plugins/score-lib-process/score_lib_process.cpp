#include <Process/Dataflow/PortFactory.hpp>
#include <Process/DocumentPlugin.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPluginCreator.hpp>

#include <score_lib_process.hpp>
#include <score_lib_process_commands_files.hpp>

score_lib_process::score_lib_process() = default;
score_lib_process::~score_lib_process() = default;

std::vector<std::unique_ptr<score::InterfaceBase>>
score_lib_process::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  using namespace Process;
  return instantiate_factories<score::ApplicationContext>(ctx, key);
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_lib_process::factoryFamilies()
{
  return make_ptr_vector<
      score::InterfaceListBase, Process::ProcessFactoryList,
      Process::PortFactoryList, Process::LayerFactoryList,
      Process::ProcessFactoryList, Process::ProcessDropHandlerList>();
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_lib_process::make_commands()
{
  using namespace Process;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_lib_process_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

score::GUIApplicationPlugin* score_lib_process::make_guiApplicationPlugin(
    const score::GUIApplicationContext& app)
{
  struct app_plug final : public score::GUIApplicationPlugin
  {
    using score::GUIApplicationPlugin::GUIApplicationPlugin;

    void on_initDocument(score::Document& doc) override
    {
      score::addDocumentPlugin<Process::DocumentPlugin>(doc);
    }
  };

  return new app_plug{app};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_lib_process)
