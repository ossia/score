#include "score_plugin_dataflow.hpp"

#include <Process/Dataflow/ControlWidgets.hpp>
#include <Process/Dataflow/PortListWidget.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <Dataflow/AudioInletItem.hpp>
#include <Dataflow/AudioOutletItem.hpp>
#include <Dataflow/CableInspector.hpp>
#include <Dataflow/ControlInletItem.hpp>
#include <Dataflow/ControlOutletItem.hpp>
#include <Dataflow/CurveInlet.hpp>
#include <Dataflow/DropPortInScenario.hpp>
#include <Dataflow/MidiInletItem.hpp>
#include <Dataflow/MidiOutletItem.hpp>
#include <Dataflow/PortInspectorFactory.hpp>
#include <Dataflow/PortItem.hpp>
#include <Dataflow/ValueInletItem.hpp>
#include <Dataflow/ValueOutletItem.hpp>
#include <Dataflow/WidgetInletFactory.hpp>

#include <score/plugins/FactorySetup.hpp>

score_plugin_dataflow::score_plugin_dataflow() { }

score_plugin_dataflow::~score_plugin_dataflow() { }

std::vector<score::InterfaceBase*> score_plugin_dataflow::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  return instantiate_factories<
      score::ApplicationContext,
      FW<Inspector::InspectorWidgetFactory, Dataflow::CableInspectorFactory,
         Dataflow::InletInspectorFactory, Dataflow::OutletInspectorFactory>,
      FW<Scenario::IntervalDropHandler, Dataflow::DropPortInInterval>,
      FW<Scenario::DropHandler, Dataflow::DropPortInScenario>,
      FW<Process::PortFactory, Dataflow::ControlInletFactory, Dataflow::MidiInletFactory,
         Dataflow::MidiOutletFactory, Dataflow::ValueInletFactory,
         Dataflow::ValueOutletFactory, Dataflow::AudioInletFactory,
         Dataflow::AudioOutletFactory, Dataflow::ControlOutletFactory,
         Dataflow::MinMaxFloatOutletFactory,
         Dataflow::WidgetInletFactory<Process::FloatSlider, WidgetFactory::FloatSlider>,
         Dataflow::WidgetInletFactory<Process::FloatKnob, WidgetFactory::FloatKnob>,
         Dataflow::WidgetInletFactory<
             Process::LogFloatSlider, WidgetFactory::LogFloatSlider>,
         Dataflow::WidgetInletFactory<Process::IntSlider, WidgetFactory::IntSlider>,
         Dataflow::WidgetInletFactory<
             Process::IntRangeSlider, WidgetFactory::IntRangeSlider>,
         Dataflow::WidgetInletFactory<
             Process::FloatRangeSlider, WidgetFactory::FloatRangeSlider>,
         Dataflow::WidgetInletFactory<
             Process::IntRangeSpinBox, WidgetFactory::FloatRangeSpinBox>,
         Dataflow::WidgetInletFactory<
             Process::FloatRangeSpinBox, WidgetFactory::FloatRangeSpinBox>,
         Dataflow::WidgetInletFactory<Process::IntSpinBox, WidgetFactory::IntSpinBox>,
         Dataflow::WidgetInletFactory<
             Process::FloatSpinBox, WidgetFactory::FloatSpinBox>,
         Dataflow::WidgetInletFactory<Process::TimeChooser, WidgetFactory::TimeChooser>,
         Dataflow::WidgetInletFactory<Process::Toggle, WidgetFactory::Toggle>,
         Dataflow::WidgetInletFactory<Process::Button, WidgetFactory::Button>,
         Dataflow::WidgetInletFactory<
             Process::ImpulseButton, WidgetFactory::ImpulseButton>,
         Dataflow::WidgetInletFactory<
             Process::ChooserToggle, WidgetFactory::ChooserToggle>,
         Dataflow::WidgetInletFactory<Process::LineEdit, WidgetFactory::LineEdit>,
         Dataflow::WidgetInletFactory<Process::ProgramEdit, WidgetFactory::ProgramEdit>,
         Dataflow::WidgetInletFactory<Process::FileChooser, WidgetFactory::FileChooser>,
         Dataflow::WidgetInletFactory<
             Process::VideoFileChooser, WidgetFactory::FileChooser>,
         Dataflow::WidgetInletFactory<Process::ComboBox, WidgetFactory::ComboBox>,
         Dataflow::WidgetInletFactory<Process::Enum, WidgetFactory::Enum>,
         Dataflow::WidgetInletFactory<Process::HSVSlider, WidgetFactory::HSVSlider>,
         Dataflow::WidgetInletFactory<Process::XYSlider, WidgetFactory::XYSlider>,
         Dataflow::WidgetInletFactory<Process::XYZSlider, WidgetFactory::XYZSlider>,
         Dataflow::WidgetInletFactory<Process::XYSpinboxes, WidgetFactory::XYSpinboxes>,
         Dataflow::WidgetInletFactory<
             Process::XYZSpinboxes, WidgetFactory::XYZSpinboxes>,
         Dataflow::WidgetInletFactory<Process::MultiSlider, WidgetFactory::MultiSlider>,
         Dataflow::WidgetInletFactory<Process::MultiSliderXY, WidgetFactory::MultiSliderXY>,
         Dataflow::WidgetInletFactory<
             Dataflow::CurveInlet, WidgetFactory::CurveInletItems>,
         Dataflow::WidgetOutletFactory<Process::Bargraph, WidgetFactory::Bargraph>>>(
      ctx, key);
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_dataflow)
