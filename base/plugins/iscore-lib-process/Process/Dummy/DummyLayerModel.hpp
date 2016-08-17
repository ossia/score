#pragma once
#include <Process/Dummy/DummyLayerModel.hpp>
#include <Process/LayerModel.hpp>

namespace Dummy
{
using Layer = Process::LayerModel_T<Process::ProcessModel>;
}

DEFAULT_MODEL_METADATA(Dummy::Layer, "DummyLayerModel")
