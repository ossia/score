#pragma once
#include <Scenario/Commands/ScriptEditCommand.hpp>

#include <Faust/EffectModel.hpp>
namespace Faust
{
inline const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Faust"};
  return key;
}

class EditScript
    : public Scenario::EditScript<FaustEffectModel, FaustEffectModel::p_script>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), EditScript, "Edit a Faust program")
public:
  using Scenario::EditScript<FaustEffectModel, FaustEffectModel::p_script>::EditScript;
};
}

namespace score
{
template <>
struct StaticPropertyCommand<Faust::FaustEffectModel::p_script> : Faust::EditScript
{
  using Faust::EditScript::EditScript;
};
}
