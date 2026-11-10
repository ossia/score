// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_ysfx.hpp"

#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/ProcessFactory.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>
#include <YSFX/ApplicationPlugin.hpp>
#include <YSFX/Commands/CommandFactory.hpp>
#include <YSFX/Executor/Component.hpp>
#include <YSFX/ProcessFactory.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>

#include <QFile>
#include <QFileInfo>
#include <QQmlListProperty>
#include <QTimer>

#include <score_plugin_ysfx_commands_files.hpp>
#include <wobjectimpl.h>

namespace YSFX
{
//! A file without the .jsfx extension, as REAPER's own effects are, is an
//! effect when it is text with a desc: line, which REAPER lists effects by,
//! in its first 16 kB: REAPER's and ATK's effects have it in their first 4 kB,
//! after a comment block at most.
static bool isExtensionlessJsfx(std::string_view path)
{
  QFile f{QString::fromUtf8(path.data(), path.size())};
  if(!f.open(QIODevice::ReadOnly))
    return false;
  const QByteArray head = f.read(16384);
  if(head.contains('\0'))
    return false;

  for(qsizetype line = 0; line < head.size();)
  {
    qsizetype i = line;
    while(i < head.size() && (head[i] == ' ' || head[i] == '\t'))
      i++;
    if(QByteArrayView{head}.sliced(i).startsWith("desc:"))
      return true;
    const auto next = head.indexOf('\n', i);
    if(next < 0)
      break;
    line = next + 1;
  }
  return false;
}

class LibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("95cc72d2-ab43-47fc-ad16-b8f7ef34a1e1")

  //! "": the files whose name has no extension
  QSet<QString> acceptedFiles() const noexcept override { return {"jsfx", ""}; }

  Library::CategoryPaths categories;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    categories.init(
        Metadata<PrettyName_k, YSFX::ProcessModel>::get().toStdString(), ctx);
  }

  std::optional<Library::ProcessEntry> scanPath(std::string_view path) override
  {
    score::PathInfo file{path};
    const bool extension = file.fileName.ends_with(".jsfx");
    if(!extension && !isExtensionlessJsfx(path))
      return std::nullopt;

    const auto name = extension ? file.completeBaseName : file.fileName;
    Library::ProcessData pdata;
    pdata.prettyName = QString::fromUtf8(name.data(), name.size());
    pdata.key = Metadata<ConcreteKey_k, YSFX::ProcessModel>::get();
    pdata.customData
        = QString::fromUtf8(file.absoluteFilePath.data(), file.absoluteFilePath.size());

    return Library::ProcessEntry{pdata.key, categories(file), {std::move(pdata), {}}};
  }
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("8c4a1379-2a15-4833-83b4-c393f14d32db")

  QSet<QString> fileExtensions() const noexcept override { return {"jsfx"}; }

  void dropPath(
      std::vector<ProcessDrop>& vec, const score::FilePath& filename,
      const score::DocumentContext& ctx) const noexcept override
  {
    Process::ProcessDropHandler::ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, ProcessModel>::get();
    p.creation.prettyName = filename.basename;
    p.creation.customData = filename.relative;

    vec.push_back(std::move(p));
  }
};

}

score_plugin_ysfx::score_plugin_ysfx() { }

score_plugin_ysfx::~score_plugin_ysfx() = default;

std::vector<score::InterfaceBase*> score_plugin_ysfx::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext, FW<Process::ProcessModelFactory, YSFX::ProcessFactory>,
      FW<Process::LayerFactory, YSFX::LayerFactory>,
      FW<Library::LibraryInterface, YSFX::LibraryHandler>,
      FW<Process::ProcessDropHandler, YSFX::DropHandler>,
      FW<Execution::ProcessComponentFactory, YSFX::Executor::ComponentFactory>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_ysfx::make_commands()
{
  using namespace YSFX;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      YSFX::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_ysfx_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

score::ApplicationPlugin*
score_plugin_ysfx::make_applicationPlugin(const score::ApplicationContext& app)
{
  return new YSFX::ApplicationPlugin{app};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_ysfx)
