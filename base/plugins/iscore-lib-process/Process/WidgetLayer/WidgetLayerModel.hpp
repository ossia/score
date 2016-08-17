#pragma once
#include <Process/WidgetLayer/WidgetLayerModel.hpp>
#include <Process/LayerModel.hpp>

namespace WidgetLayer
{
using Layer = Process::LayerModel_T<Process::ProcessModel>;
}

DEFAULT_MODEL_METADATA(WidgetLayer::Layer, "WidgetLayer")

