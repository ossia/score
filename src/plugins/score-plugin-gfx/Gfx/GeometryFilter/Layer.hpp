#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Script/MultiScriptEditor.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <Gfx/GeometryFilter/Process.hpp>

namespace Gfx::GeometryFilter
{
struct LanguageSpec
{
  static constexpr const char* language = "GLSL";
};
using LayerFactory = Process::ScriptLayerFactory_T<
    Gfx::GeometryFilter::Model,
    Process::ProcessScriptEditDialog<
        Gfx::GeometryFilter::Model, Gfx::GeometryFilter::Model::p_script, LanguageSpec>>;
}
