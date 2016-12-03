#pragma once
#include <Process/LayerModel.hpp>
#include <Process/WidgetLayer/WidgetLayerModel.hpp>

namespace WidgetLayer
{
using Layer = Process::LayerModel_T<Process::ProcessModel>;
}

DEFAULT_MODEL_METADATA(WidgetLayer::Layer, "WidgetLayer")
