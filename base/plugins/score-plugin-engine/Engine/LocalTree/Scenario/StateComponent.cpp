// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "StateComponent.hpp"
#include "MetadataParameters.hpp"
#include <ossia/editor/state/state_element.hpp>
#include <Scenario/Application/ScenarioApplicationPlugin.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Engine/Executor/DocumentPlugin.hpp>
#include <Engine/score2OSSIA.hpp>
namespace Engine
{
namespace LocalTree
{

State::State(
    ossia::net::node_base& parent,
    const Id<score::Component>& id,
    Scenario::StateModel& state,
    DocumentPlugin& doc,
    QObject* parent_comp)
    : CommonComponent{parent, state.metadata(), doc,
                      id,     "StateComponent", parent_comp}
{
  m_properties.push_back(add_setProperty<::State::impulse>(
      node(), "trigger", [&] (auto) {
    auto& r_ctx
        = doc.context().plugin<Engine::Execution::DocumentPlugin>().context();

    auto ossia_state
        = Engine::score_to_ossia::state(state, r_ctx);
    ossia_state.launch();
  }));
}
}
}
