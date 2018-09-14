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
#include <Process/ProcessFactory.hpp>

#include <score/plugins/customfactory/FactoryFamily.hpp>
#include <score/plugins/customfactory/FactorySetup.hpp>
#include <score/plugins/customfactory/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <score_plugin_js_commands_files.hpp>

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

  qRegisterMetaType<QVector<JS::MidiMessage>>();
}

score_plugin_js::~score_plugin_js() = default;

std::vector<std::unique_ptr<score::InterfaceBase>> score_plugin_js::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Process::ProcessModelFactory, JS::ProcessFactory>,
      FW<Process::LayerFactory, JS::LayerFactory>,
      FW<Inspector::InspectorWidgetFactory, JS::InspectorFactory>,
      FW<score::PanelDelegateFactory, JS::PanelDelegateFactory>,
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
