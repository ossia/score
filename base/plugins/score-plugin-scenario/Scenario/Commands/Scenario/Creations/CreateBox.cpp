#include "CreateBox.hpp"

void Scenario::Command::MakeBox(
    const score::CommandStackFacade& stack,
    const Scenario::ProcessModel& scenario,
    TimeVal date,
    TimeVal endDate,
    double Y)
{
  RedoMacroCommandDispatcher<CreateBox> disp{stack};
  auto cmd1 = new CreateTimeSync_Event_State{scenario, date, Y};
  disp.submitCommand(cmd1);
  disp.submitCommand(new CreateInterval_State_Event_TimeSync{
      scenario, cmd1->createdState(), endDate, Y});
  disp.commit();
}
