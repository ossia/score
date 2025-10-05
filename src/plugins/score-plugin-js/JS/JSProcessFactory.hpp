#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/Script/ScriptEditor.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <JS/JSProcessMetadata.hpp>
#include <JS/JSProcessModel.hpp>

namespace JS
{
struct LanguageSpec
{
  static constexpr const char* language = "JS";
};

using ProcessFactory = Process::ProcessFactory_T<JS::ProcessModel>;
using LayerFactory = Process::EffectLayerFactory_T<
    ProcessModel, Process::ProcessScriptEditDialog<
                      ProcessModel, ProcessModel::p_script, LanguageSpec>>;
}
