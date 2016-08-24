#pragma once
#include <Interpolation/InterpolationProcess.hpp>
#include <Process/LayerModel.hpp>

namespace Interpolation
{
using Layer = Process::LayerModel_T<ProcessModel>;
}
DEFAULT_MODEL_METADATA(Interpolation::Layer, "InterpLayer")
