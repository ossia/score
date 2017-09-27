#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Media/Input/InputModel.hpp>
#include <Media/Input/InputMetadata.hpp>

namespace Media
{
namespace Input
{
using ProcessFactory = Process::GenericProcessModelFactory<Input::ProcessModel>;
using LayerFactory = Process::GenericDefaultLayerFactory<Input::ProcessModel>;
}
}
