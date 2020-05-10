#pragma once
#include <JS/JSProcessModel.hpp>
#include <JS/Commands/JSCommandFactory.hpp>
#include <Scenario/Commands/ScriptEditCommand.hpp>
namespace JS
{

class EditScript
    : public Scenario::EditScript<JS::ProcessModel, JS::ProcessModel::p_script>
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      EditScript,
      "Edit a script")
  public:
    using Scenario::EditScript<JS::ProcessModel, JS::ProcessModel::p_script>::EditScript;
};
}

namespace score
{
template<>
struct StaticPropertyCommand<JS::ProcessModel::p_script> : JS::EditScript
{
  using JS::EditScript::EditScript;
};
}

