#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Script/MultiScriptEditor.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>

#include <Threedim/RenderPipeline/Process.hpp>

namespace Gfx::RenderPipeline
{
using LayerFactory = Process::ScriptLayerFactory_T<
    Gfx::RenderPipeline::Model,
    Process::ProcessMultiScriptEditDialog<
        Gfx::RenderPipeline::Model, Gfx::RenderPipeline::Model::p_program>>;
}
