// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_js.hpp"

#include "JS/Commands/JSCommandFactory.hpp"

#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/ProcessFactory.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <JS/ConsolePanel.hpp>
#include <JS/Executor/Component.hpp>
#include <JS/JSProcessFactory.hpp>
#include <JS/Qml/QmlObjects.hpp>
#include <JS/Qml/ValueTypes.hpp>
#include <Library/LibraryInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <Library/ProcessesItemModel.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>

#include <QFileInfo>
#include <QQmlListProperty>
#include <QTimer>

#include <score_plugin_js_commands_files.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(JS::JsUtils)
W_OBJECT_IMPL(JS::EditJsContext)
W_OBJECT_IMPL(JS::ActionContext)
namespace JS
{

class ModuleLibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("6e72e377-efdd-4e3c-9900-922b618e7d70")

public:
  JS::PanelDelegate* panel{};

  QSet<QString> acceptedFiles() const noexcept override { return {"mjs"}; }

  bool add(const QString& path)
  {
    QFile f{path};
    if(!f.open(QIODevice::ReadOnly))
      return false;

    if(!panel)
      panel = &score::GUIAppContext().panel<JS::PanelDelegate>();

    panel->importModule(path);
    return true;
  }

  void addPath(std::string_view path) override
  {
    add(QString::fromUtf8(path.data(), path.length()));
  }

  bool onDoubleClick(const QString& path, const score::DocumentContext& ctx) override
  {
    return add(path);
  }
};

class ConsoleLibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("21f405da-a249-4e39-b405-9173aff11b26")

  QSet<QString> acceptedFiles() const noexcept override { return {"js"}; }

  bool onDoubleClick(const QString& path, const score::DocumentContext& ctx) override
  {
    QFile f{path};
    if(!f.open(QIODevice::ReadOnly))
      return false;

    auto& p = ctx.app.panel<JS::PanelDelegate>();
    if(QFileInfo{f}.suffix() == "mjs")
    {
      p.importModule(path);
    }
    else
    {
      auto data = f.readAll();
      p.evaluate(data);
    }
    return true;
  }
};

class LibraryHandler final
    : public QObject
    , public Library::LibraryInterface
{
  SCORE_CONCRETE("5231ea8b-da66-4c6f-9e34-d9a79cbc494a")

  QSet<QString> acceptedFiles() const noexcept override { return {"qml"}; }

  static inline const QRegularExpression scoreImport{"import Score [0-9].[0-9]"};

  Library::Subcategories categories;

  void setup(Library::ProcessesItemModel& model, const score::GUIApplicationContext& ctx)
      override
  {
    // TODO relaunch whenever library path changes...
    const auto& key = Metadata<ConcreteKey_k, JS::ProcessModel>::get();
    QModelIndex node = model.find(key);
    if(node == QModelIndex{})
      return;

    categories.init(node, ctx);
  }

  void addPath(std::string_view path) override
  {
    QFileInfo file{QString::fromUtf8(path.data(), path.length())};
    Library::ProcessData pdata;
    pdata.prettyName = file.completeBaseName();
    pdata.key = Metadata<ConcreteKey_k, JS::ProcessModel>::get();
    pdata.customData = [&] {
      QFile f(file.absoluteFilePath());
      f.open(QIODevice::ReadOnly);
      return f.readAll().trimmed();
    }();

    {
      auto matches = scoreImport.match(pdata.customData);
      if(matches.hasMatch())
      {
        categories.add(file, std::move(pdata));
      }
    }
  }
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("ad3a575a-f4a8-4a89-bb7e-bfd85f3430fe")

  QSet<QString> fileExtensions() const noexcept override { return {"qml"}; }

  void dropData(
      std::vector<ProcessDrop>& vec, const DroppedFile& data,
      const score::DocumentContext& ctx) const noexcept override
  {
    const auto& [filename, file] = data;
    Process::ProcessDropHandler::ProcessDrop p;
    p.creation.key = Metadata<ConcreteKey_k, ProcessModel>::get();
    p.creation.prettyName = QFileInfo{filename}.baseName();
    p.creation.customData = std::move(file);

    vec.push_back(std::move(p));
  }
};

}
W_OBJECT_IMPL(JS::Script)

score_plugin_js::score_plugin_js()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  JS::registerQmlValueTypeProvider();
#endif

  qmlRegisterType<JS::ControlInlet>("Score", 1, 0, "ControlInlet");
  qmlRegisterType<JS::ValueInlet>("Score", 1, 0, "ValueInlet");
  qmlRegisterType<JS::ValueOutlet>("Score", 1, 0, "ValueOutlet");
  qmlRegisterType<JS::AudioInlet>("Score", 1, 0, "AudioInlet");
  qmlRegisterType<JS::AudioOutlet>("Score", 1, 0, "AudioOutlet");
  qmlRegisterType<JS::MidiInlet>("Score", 1, 0, "MidiInlet");
  qmlRegisterType<JS::MidiOutlet>("Score", 1, 0, "MidiOutlet");
  qmlRegisterType<JS::FloatSlider>("Score", 1, 0, "FloatSlider");
  qmlRegisterType<JS::IntSlider>("Score", 1, 0, "IntSlider");
  qmlRegisterType<JS::Enum>("Score", 1, 0, "Enum");
  qmlRegisterType<JS::Toggle>("Score", 1, 0, "Toggle");
  qmlRegisterType<JS::LineEdit>("Score", 1, 0, "LineEdit");
  qmlRegisterType<JS::Script>("Score", 1, 0, "Script");

  qRegisterMetaType<QVector<JS::MidiMessage>>();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  qRegisterMetaType<JS::SampleTimings>();
  qRegisterMetaType<JS::TokenRequestValueType>();
  qRegisterMetaType<JS::ExecutionStateValueType>();
#endif
}

score_plugin_js::~score_plugin_js() = default;

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_js::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext, FW<Process::ProcessModelFactory, JS::ProcessFactory>,
      FW<Process::LayerFactory, JS::LayerFactory>,
      FW<score::PanelDelegateFactory, JS::PanelDelegateFactory>,
      FW<Library::LibraryInterface, JS::LibraryHandler, JS::ConsoleLibraryHandler,
         JS::ModuleLibraryHandler>,
      FW<Process::ProcessDropHandler, JS::DropHandler>,
      FW<Execution::ProcessComponentFactory, JS::Executor::ComponentFactory>>(ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap> score_plugin_js::make_commands()
{
  using namespace JS;
  std::pair<const CommandGroupKey, CommandGeneratorMap> cmds{
      JS::CommandFactoryName(), CommandGeneratorMap{}};

  ossia::for_each_type<
#include <score_plugin_js_commands.hpp>
      >(score::commands::FactoryInserter{cmds.second});

  return cmds;
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_js)
