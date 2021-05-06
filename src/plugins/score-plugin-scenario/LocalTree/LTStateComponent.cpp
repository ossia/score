// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateComponent.hpp"

#include <LocalTree/SetProperty.hpp>

#include <ossia/editor/state/state_element.hpp>

#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>

namespace LocalTree
{

State::State(
    ossia::net::node_base& parent,
    Scenario::StateModel& state,
    const score::DocumentContext& doc,
    QObject* parent_comp)
    : CommonComponent{
        parent,
        state.metadata(),
        doc,
        "StateComponent",
        parent_comp}
{
  m_properties.push_back(add_setProperty<::State::impulse>(
      node(),
      "trigger",
      [&doc, s = QPointer<Scenario::StateModel>(&state)](auto) {
        if (s)
        {
          auto plug = doc.app.findGuiApplicationPlugin<
              Scenario::ScenarioApplicationPlugin>();
          if (plug)
          {
            plug->execution().playState(
                &Scenario::parentScenario(*s), s->id());
          }
        }
      }));
}

State::~State() { }
}
