#pragma once
#include <Mapping/MappingModel.hpp>
#include <Process/LayerModel.hpp>

namespace Mapping
{
using Layer = Process::LayerModel_T<ProcessModel>;
}

LAYER_METADATA(
        ISCORE_PLUGIN_MAPPING_EXPORT,
        Mapping::Layer,
        "a4834de8-f89d-46a5-b744-48761d91e6e6",
        "MappingLayer",
        "MappingLayer"
        )
