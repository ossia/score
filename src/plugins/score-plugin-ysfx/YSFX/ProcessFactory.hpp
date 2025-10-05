#pragma once
#include <Process/GenericProcessFactory.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/Script/ScriptEditor.hpp>

#include <Control/DefaultEffectItem.hpp>
#include <Effect/EffectFactory.hpp>
#include <YSFX/ProcessMetadata.hpp>
#include <YSFX/ProcessModel.hpp>

namespace YSFX
{
struct LanguageSpec
{
  static constexpr const char* language = "YSFX";
};

using ProcessFactory = Process::ProcessFactory_T<YSFX::ProcessModel>;
}
