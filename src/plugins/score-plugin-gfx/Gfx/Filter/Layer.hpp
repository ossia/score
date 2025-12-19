#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Script/MultiScriptEditor.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <Gfx/Filter/Process.hpp>

namespace Gfx::Filter
{
using LayerFactory = Process::ScriptLayerFactory_T<
    Gfx::Filter::Model, Process::ProcessMultiScriptEditDialog<
                            Gfx::Filter::Model, Gfx::Filter::Model::p_program>>;
}
