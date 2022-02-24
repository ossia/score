// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_ysfx.hpp"

#include <YSFX/Commands/CommandFactory.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <YSFX/Executor/Component.hpp>
#include <YSFX/ProcessFactory.hpp>
#include <YSFX/ApplicationPlugin.hpp>
#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/ProcessFactory.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>

#include <QFileInfo>
#include <QQmlListProperty>
#include <QTimer>

#include <score_plugin_ysfx_commands_files.hpp>
#include <wobjectimpl.h>

namespace YSFX
{
class LibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("95cc72d2-ab43-47fc-ad16-b8f7ef34a1e1")

  QSet<QString> acceptedFiles() const noexcept override
  {
    return {"jsfx"};
  }

  Library::Subcategories categories;

  void setup(
      Library::ProcessesItemModel& model,
      const score::GUIApplicationContext& ctx) override
  {
    // TODO relaunch whenever library path changes...
    const auto& key = Metadata<ConcreteKey_k, YSFX::ProcessModel>::get();
    QModelIndex node = model.find(key);
    if (node == QModelIndex{})
      return;

    categories.init(node, ctx);
  }

  void addPath(std::string_view path) override
  {
    QFileInfo file{QString::fromUtf8(path.data(), path.length())};
    Library::ProcessData pdata;
    pdata.prettyName = file.completeBaseName();

    pdata.key = Metadata<ConcreteKey_k, YSFX::ProcessModel>::get();
    pdata.customData = file.absoluteFilePath();
    categories.add(file, std::move(pdata));
  }
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("8c4a1379-2a15-4833-83b4-c393f14d32db")

  QSet<QString> fileExtensions() const noexcept override
  {
    return {"jsfx"};
  }

  void dropPath(
        std::vector<ProcessDrop>& vec,
        const QString& filename,
        const score::DocumentContext& ctx) const noexcept override
  {
    QFileInfo finfo{filename};
    Process::ProcessDropHandler::ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, ProcessModel>::get();
    p.creation.prettyName = finfo.completeBaseName();
    p.creation.customData = finfo.absoluteFilePath();

    vec.push_back(std::move(p));
  }
};

}

score_plugin_ysfx::score_plugin_ysfx()
{

}

score_plugin_ysfx::~score_plugin_ysfx() = default;

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_ysfx::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, YSFX::ProcessFactory>,
      FW<Process::LayerFactory, YSFX::LayerFactory>,
      FW<Library::LibraryInterface, YSFX::LibraryHandler>,
      FW<Process::ProcessDropHandler, YSFX::DropHandler>,
      FW<Execution::ProcessComponentFactory, YSFX::Executor::ComponentFactory>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_ysfx::make_commands()
{
  using namespace YSFX;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      YSFX::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_ysfx_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

score::ApplicationPlugin* score_plugin_ysfx::make_applicationPlugin(const score::ApplicationContext& app)
{
  return new YSFX::ApplicationPlugin{app};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_ysfx)

