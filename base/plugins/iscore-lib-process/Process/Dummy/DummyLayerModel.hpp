#pragma once
#include <Process/LayerModel.hpp>
#include <Process/ProcessMetadata.hpp>

namespace Dummy
{
using Layer = Process::LayerModel_T<Process::ProcessModel>;
}

LAYER_METADATA(
        ,
        Dummy::Layer,
        "be01601e-f1b0-43e2-b870-38559d589b5d",
        "DummyLayerModel",
        "DummyLayerModel"
        )

