#pragma once
#include <Faust/EffectModel.hpp>
#include <Scenario/Commands/ScriptEditCommand.hpp>
namespace Faust
{
inline
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Faust"};
  return key;
}

class EditScript : public Scenario::EditScript<FaustEffectModel, FaustEffectModel::p_text>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), EditScript, "Edit a Faust program")
public:
  using Scenario::EditScript<FaustEffectModel, FaustEffectModel::p_text>::EditScript;
};
}

namespace score
{
template <>
struct StaticPropertyCommand<Faust::FaustEffectModel::p_text> : Faust::EditScript
{
  using Faust::EditScript::EditScript;
};
}
