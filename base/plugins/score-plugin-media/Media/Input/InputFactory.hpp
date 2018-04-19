#pragma once
#include <Media/Input/InputMetadata.hpp>
#include <Media/Input/InputModel.hpp>
#include <Process/GenericProcessFactory.hpp>

namespace Media
{
namespace Input
{
using ProcessFactory = Process::ProcessFactory_T<Input::ProcessModel>;
using LayerFactory = Process::GenericDefaultLayerFactory<Input::ProcessModel>;
}
}
