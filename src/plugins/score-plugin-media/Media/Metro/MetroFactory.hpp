#pragma once
#include <Process/GenericProcessFactory.hpp>

#include <Effect/EffectFactory.hpp>
#include <Media/Metro/MetroMetadata.hpp>
#include <Media/Metro/MetroModel.hpp>

namespace Media
{
namespace Metro
{
using ProcessFactory = Process::ProcessFactory_T<Metro::Model>;
//! The controls are the whole interface.
using LayerFactory = Process::EffectLayerFactory_T<Metro::Model>;
}
}
