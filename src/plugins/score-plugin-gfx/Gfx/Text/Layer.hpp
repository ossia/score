#pragma once
#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <Gfx/Text/Process.hpp>
#include <Process/GenericProcessFactory.hpp>

namespace Gfx::Text
{
using LayerFactory = Process::
    EffectLayerFactory_T<Gfx::Text::Model, Process::DefaultEffectItem>;
}
