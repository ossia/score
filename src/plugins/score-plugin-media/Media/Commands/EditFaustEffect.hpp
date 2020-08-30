#pragma once
#include <Media/Commands/MediaCommandFactory.hpp>
#include <Media/Effect/Faust/FaustEffectModel.hpp>
#include <Scenario/Commands/ScriptEditCommand.hpp>
namespace Media::Faust
{
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
struct StaticPropertyCommand<Media::Faust::FaustEffectModel::p_text> : Media::Faust::EditScript
{
  using Media::Faust::EditScript::EditScript;
};
}
