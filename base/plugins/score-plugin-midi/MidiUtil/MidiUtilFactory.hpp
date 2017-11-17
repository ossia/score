#pragma once
#include <MidiUtil/MidiUtilProcess.hpp>
#include <MidiUtil/Inspector/Inspector.hpp>
#include <Process/GenericProcessFactory.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>
#include <Process/LayerModelPanelProxy.hpp>

namespace MidiUtil
{
using ProcessFactory = Process::GenericProcessModelFactory<MidiUtil::ProcessModel>;
using LayerFactory = WidgetLayer::LayerFactory<MidiUtil::ProcessModel, MidiUtil::InspectorWidget>;
}
