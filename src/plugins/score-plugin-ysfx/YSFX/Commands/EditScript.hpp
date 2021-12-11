#pragma once
#include <YSFX/Commands/CommandFactory.hpp>
#include <YSFX/ProcessModel.hpp>

#include <Scenario/Commands/ScriptEditCommand.hpp>

//  namespace YSFX
//  {
//  class EditYSFXScript
//      : public Scenario::EditScript<YSFX::ProcessModel, YSFX::ProcessModel::p_script>
//  {
//    SCORE_COMMAND_ DECL(CommandFactoryName(), EditYSFXScript, "Edit a script")
//  public:
//    using EditScript::EditScript;
//  };
//  }
//
//  namespace score
//  {
//  template <>
//  struct StaticPropertyCommand<YSFX::ProcessModel::p_script> : YSFX::EditYSFXScript
//  {
//    using EditYSFXScript::EditYSFXScript;
//  };
//  }
//
