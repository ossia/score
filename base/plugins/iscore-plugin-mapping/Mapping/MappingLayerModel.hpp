#pragma once
#include <Mapping/MappingModel.hpp>
#include <Process/LayerModel.hpp>

namespace Mapping
{
using Layer = Process::LayerModel_T<ProcessModel>;
}
DEFAULT_MODEL_METADATA(Mapping::Layer, "Mapping layer")
