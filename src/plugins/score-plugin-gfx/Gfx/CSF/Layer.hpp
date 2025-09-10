#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/Script/ScriptEditor.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <Gfx/CSF/Process.hpp>

namespace Gfx::CSF
{
class Model;
struct LanguageSpec
{
  static constexpr const char* language = "GLSL";
};
using LayerFactory = Process::EffectLayerFactory_T<
    Gfx::CSF::Model, Process::DefaultEffectItem,
    Process::ProcessScriptEditDialog<
        Gfx::CSF::Model, Gfx::CSF::Model::p_compute, LanguageSpec>>;
}
