#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/WidgetLayer/WidgetProcessFactory.hpp>

#include <JS/Inspector/JSInspectorWidget.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <JS/JSProcessModel.hpp>
#include <JS/JSStateProcess.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/StateProcessFactory.hpp>

namespace JS
{
using ProcessFactory = Process::GenericProcessModelFactory<JS::ProcessModel>;
using LayerFactory = WidgetLayer::LayerFactory<JS::ProcessModel, JS::InspectorWidget>;

using StateProcessFactory = Process::StateProcessFactory_T<JS::StateProcess>;
}
