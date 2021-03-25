#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>

#include <Pd/Inspector/PdInspectorWidget.hpp>
#include <Pd/PdProcess.hpp>
namespace Pd
{
using LayerFactory = WidgetLayer::LayerFactory<Pd::ProcessModel, PdWidget>;
}
