#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Script/MultiScriptEditor.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <Gfx/Filter/Process.hpp>

namespace Gfx::Filter
{
using LayerFactory = Process::EffectLayerFactory_T<
    Gfx::Filter::Model,
    Process::DefaultEffectItem,
    Process::ProcessMultiScriptEditDialog<Gfx::Filter::Model, Gfx::Filter::Model::p_program>>;
}
