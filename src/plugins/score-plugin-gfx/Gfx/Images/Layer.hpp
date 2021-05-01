#pragma once
#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <Gfx/Images/Process.hpp>
#include <Process/GenericProcessFactory.hpp>

namespace Gfx::Images
{
using LayerFactory = Process::
    EffectLayerFactory_T<Gfx::Images::Model, Process::DefaultEffectItem>;
}
