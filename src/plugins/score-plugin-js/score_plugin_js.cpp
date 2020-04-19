// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_js.hpp"

#include "JS/Commands/JSCommandFactory.hpp"

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <JS/ConsolePanel.hpp>
#include <JS/Executor/Component.hpp>
#include <JS/Inspector/JSInspectorFactory.hpp>
#include <JS/JSProcessFactory.hpp>
#include <JS/Qml/QmlObjects.hpp>
#include <QQmlListProperty>
#include <Library/LibraryInterface.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>
#include <Process/ProcessFactory.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <score_plugin_js_commands_files.hpp>

#include <QFileInfo>
#include <wobjectimpl.h>

W_OBJECT_IMPL(JS::EditJsContext)

namespace JS
{
class LibraryHandler final : public Library::LibraryInterface
{
  SCORE_CONCRETE("5231ea8b-da66-4c6f-9e34-d9a79cbc494a")

  QSet<QString> acceptedFiles() const noexcept override
  {
    return {"js", "qml"};
  }
};

class DropHandler final : public Process::ProcessDropHandler
{
  SCORE_CONCRETE("ad3a575a-f4a8-4a89-bb7e-bfd85f3430fe")

  QSet<QString> fileExtensions() const noexcept override
  {
    return {"js", "qml"};
  }

  std::vector<Process::ProcessDropHandler::ProcessDrop> dropData(
      const std::vector<DroppedFile>& data,
      const score::DocumentContext& ctx) const noexcept override
  {
    std::vector<Process::ProcessDropHandler::ProcessDrop> vec;

    for (auto&& [filename, file] : data)
    {
      Process::ProcessDropHandler::ProcessDrop p;
      p.creation.key = Metadata<ConcreteKey_k, ProcessModel>::get();
      p.creation.prettyName = QFileInfo{filename}.baseName();
      p.creation.customData = std::move(file);

      vec.push_back(std::move(p));
    }

    return vec;
  }
};

}
W_OBJECT_IMPL(JS::Script)

score_plugin_js::score_plugin_js()
{
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
}

score_plugin_js::~score_plugin_js() = default;

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_js::factories(
    const score::ApplicationContext& ctx,
    const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, JS::ProcessFactory>,
      FW<Process::LayerFactory, JS::LayerFactory>,
      FW<Inspector::InspectorWidgetFactory, JS::InspectorFactory>,
      FW<score::PanelDelegateFactory, JS::PanelDelegateFactory>,
      FW<Library::LibraryInterface, JS::LibraryHandler>,
      FW<Process::ProcessDropHandler, JS::DropHandler>,
      FW<Execution::ProcessComponentFactory, JS::Executor::ComponentFactory>>(
      ctx, key);
}

std::pair<const CommandGroupKey, CommandGeneratorMap>
score_plugin_js::make_commands()
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
