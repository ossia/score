#pragma once

// Processes of a test document: their controls and the base scenario.

#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>
#include <score_test/Project.hpp>

namespace score::test
{

//! The control inlet of \p p named \p name; fails the test when there is none.
inline Process::ControlInlet& control_named(Process::ProcessModel& p, const QString& name)
{
  for(auto* inlet : p.inlets())
    if(auto* ctl = qobject_cast<Process::ControlInlet*>(inlet); ctl && ctl->name() == name)
      return *ctl;
  FAIL("no control named " << name.toStdString());
  throw;
}

//! The scenario of the document's base interval.
inline Scenario::ProcessModel& base_scenario(score::Document& doc)
{
  for(auto& proc : base_interval(doc).processes)
    if(auto* s = qobject_cast<Scenario::ProcessModel*>(&proc))
      return *s;
  FAIL("no scenario in the base interval");
  throw;
}

//! A state of the base scenario, to put messages in.
inline Scenario::StateModel& some_state(score::Document& doc)
{
  auto& scenar = base_scenario(doc);
  REQUIRE(scenar.states.size() > 0);
  return *scenar.states.begin();
}

}
