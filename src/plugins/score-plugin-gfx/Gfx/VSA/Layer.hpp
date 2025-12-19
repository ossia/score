#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Script/ScriptEditor.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <Gfx/VSA/Process.hpp>

namespace Gfx::VSA
{
class Model;
struct LanguageSpec
{
  static constexpr const char* language = "GLSL";
};
using LayerFactory = Process::ScriptLayerFactory_T<
    Gfx::VSA::Model, Process::ProcessScriptEditDialog<
                         Gfx::VSA::Model, Gfx::VSA::Model::p_vertex, LanguageSpec>>;
}
