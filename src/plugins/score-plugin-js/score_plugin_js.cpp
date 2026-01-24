// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "score_plugin_js.hpp"

#include <Process/ProcessFactory.hpp>

#include <Execution/DocumentPlugin.hpp>
#include <JS/ApplicationPlugin.hpp>
#include <JS/Commands/JSCommandFactory.hpp>
#include <JS/ConsolePanel.hpp>
#include <JS/DocumentPlugin.hpp>
#include <JS/DropHandler.hpp>
#include <JS/Executor/Component.hpp>
#include <JS/JSProcessFactory.hpp>
#include <JS/LibraryHandler.hpp>
#include <JS/Qml/AddressItem.hpp>
#include <JS/Qml/DeviceEnumerator.hpp>
#include <JS/Qml/PortSink.hpp>
#include <JS/Qml/PortSource.hpp>
#include <JS/Qml/QmlObjects.hpp>
#include <JS/Qml/TextureSource.hpp>
#include <JS/Qml/Utils.hpp>
#include <JS/Qml/ValueTypes.Qt6.hpp>
#include <ossia-qt/protocols/qml_oauth.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/tools/std/HashMap.hpp>

#include <core/presenter/DocumentManager.hpp>

#include <score_plugin_js_commands_files.hpp>
#include <wobjectimpl.h>

W_OBJECT_IMPL(JS::ActionContext)
W_OBJECT_IMPL(JS::Script)
W_OBJECT_IMPL(JS::ScriptUI)

score_plugin_js::score_plugin_js()
{
  // FIXME
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  JS::registerQmlValueTypeProvider();
#endif

  ossia::qt::registerQVariantConverters();
  QMetaType::registerConverter<TimeVal, int64_t>([](const TimeVal& qval) -> int64_t {
    return qval.impl;
  });

  QMetaType::registerConverter<TimeVal, QTime>([](const TimeVal& qval) -> QTime {
    return qval.toQTime();
  });

#if QT_VERSION > QT_VERSION_CHECK(6, 10, 0)
#if QT_NETWORKAUTH_LIB
  qmlRegisterType<ossia::qt::qml_oauth>("Ossia", 1, 0, "OAuth");
#endif
#endif

  qmlRegisterType<JS::ControlInlet>("Score", 1, 0, "ControlInlet");
  qmlRegisterType<JS::ValueInlet>("Score", 1, 0, "ValueInlet");
  qmlRegisterType<JS::ValueOutlet>("Score", 1, 0, "ValueOutlet");
  qmlRegisterType<JS::AudioInlet>("Score", 1, 0, "AudioInlet");
  qmlRegisterType<JS::AudioOutlet>("Score", 1, 0, "AudioOutlet");
#if defined(SCORE_HAS_GPU_JS)
  qmlRegisterType<JS::TextureInlet>("Score", 1, 0, "TextureInlet");
  qmlRegisterType<JS::TextureOutlet>("Score", 1, 0, "TextureOutlet");
#endif
  qmlRegisterType<JS::MidiInlet>("Score", 1, 0, "MidiInlet");
  qmlRegisterType<JS::MidiOutlet>("Score", 1, 0, "MidiOutlet");
  qmlRegisterType<JS::FloatSlider<Process::FloatSlider>>("Score", 1, 0, "FloatSlider");
  qmlRegisterType<JS::FloatSlider<Process::FloatKnob>>("Score", 1, 0, "FloatKnob");
  qmlRegisterType<JS::FloatSlider<Process::FloatSpinBox>>("Score", 1, 0, "FloatSpinBox");
  qmlRegisterType<JS::IntSlider<Process::IntSlider>>("Score", 1, 0, "IntSlider");
  qmlRegisterType<JS::IntSlider<Process::IntSpinBox>>("Score", 1, 0, "IntSpinBox");

  qmlRegisterType<JS::FloatControl1D_2D<Process::FloatRangeSlider>>(
      "Score", 1, 0, "FloatRangeSlider");
  qmlRegisterType<JS::FloatControl1D_2D<Process::FloatRangeSpinBox>>(
      "Score", 1, 0, "FloatRangeSpinBox");
  qmlRegisterType<JS::FloatControl1D_2D<Process::IntRangeSlider>>(
      "Score", 1, 0, "IntRangeSlider");
  qmlRegisterType<JS::FloatControl1D_2D<Process::IntRangeSpinBox>>(
      "Score", 1, 0, "IntRangeSpinBox");
  qmlRegisterType<JS::HSVSlider>("Score", 1, 0, "HSVSlider");
  qmlRegisterType<JS::FloatControl2D<Process::XYSlider>>("Score", 1, 0, "XYSlider");
  qmlRegisterType<JS::FloatControl3D<Process::XYZSlider>>("Score", 1, 0, "XYZSlider");
  qmlRegisterType<JS::FloatControl2D<Process::XYSpinboxes>>(
      "Score", 1, 0, "XYSpinBoxes");
  qmlRegisterType<JS::FloatControl3D<Process::XYZSpinboxes>>(
      "Score", 1, 0, "XYZSpinBoxes");
  qmlRegisterType<JS::IntControl2D<Process::XYSpinboxes>>(
      "Score", 1, 0, "XYIntSpinBoxes");
  qmlRegisterType<JS::IntControl3D<Process::XYZSpinboxes>>(
      "Score", 1, 0, "XYZIntSpinBoxes");
  qmlRegisterType<JS::MultiSlider>("Score", 1, 0, "MultiSlider");
  qmlRegisterType<JS::FileChooser>("Score", 1, 0, "FileChooser");
  qmlRegisterType<JS::AudioFileChooser>("Score", 1, 0, "AudioFileChooser");
  qmlRegisterType<JS::VideoFileChooser>("Score", 1, 0, "VideoFileChooser");

  qmlRegisterType<JS::Enum>("Score", 1, 0, "Enum");
  qmlRegisterType<JS::ComboBox>("Score", 1, 0, "ComboBox");
  qmlRegisterType<JS::Toggle>("Score", 1, 0, "Toggle");
  qmlRegisterType<JS::Impulse>("Score", 1, 0, "Impulse");
  qmlRegisterType<JS::Button>("Score", 1, 0, "Button");
  qmlRegisterType<JS::LineEdit>("Score", 1, 0, "LineEdit");
  qmlRegisterType<JS::Script>("Score", 1, 0, "Script");
  qmlRegisterType<JS::ScriptUI>("Score", 1, 0, "ScriptUI");

  qmlRegisterType<JS::AddressSource>("Score.UI", 1, 0, "AddressSource");
  qmlRegisterType<JS::PortSource>("Score.UI", 1, 0, "PortSource");

#if defined(SCORE_HAS_GPU_JS)
  qmlRegisterType<JS::TextureSource>("Score.UI", 1, 0, "TextureSource");
#endif
  qmlRegisterType<JS::PortSink>("Score.UI", 1, 0, "PortSink");
  qmlRegisterType<JS::GlobalDeviceEnumerator>("Score.UI", 1, 0, "DeviceEnumerator");
  qmlRegisterType<JS::DeviceListener>("Score.UI", 1, 0, "DeviceListener");

  qRegisterMetaType<QVector<JS::MidiMessage>>();

  qRegisterMetaType<JS::SampleTimings>();
  qRegisterMetaType<JS::TokenRequestValueType>();
  qRegisterMetaType<JS::ExecutionStateValueType>();
  //qRegisterMetaType<JS::GlobalDeviceEnumerator>();

}

score_plugin_js::~score_plugin_js() = default;

std::vector<score::InterfaceBase*> score_plugin_js::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext, FW<Process::ProcessModelFactory, JS::ProcessFactory>,
      FW<score::DocumentPluginFactory, JS::DocumentPluginFactory>,
      FW<Process::LayerFactory, JS::LayerFactory>,
      FW<score::PanelDelegateFactory, JS::PanelDelegateFactory>,
      FW<Library::LibraryInterface, JS::LibraryHandler, JS::ConsoleLibraryHandler,
         JS::ModuleLibraryHandler>,
      FW<Process::ProcessDropHandler, JS::DropHandler>,
      FW<Execution::ProcessComponentFactory, JS::Executor::ComponentFactory>>(ctx, key);
}

score::GUIApplicationPlugin*
score_plugin_js::make_guiApplicationPlugin(const score::GUIApplicationContext& app)
{
  return new JS::ApplicationPlugin{app};
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
