#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>

#include <JS/Inspector/JSInspectorWidget.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <JS/JSProcessModel.hpp>
#include <Process/ProcessFactory.hpp>

namespace JS
{
using ProcessFactory = Process::ProcessFactory_T<JS::ProcessModel>;
using LayerFactory = WidgetLayer::LayerFactory<JS::ProcessModel, JS::InspectorWidget>;
}
