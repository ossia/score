// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <algorithm>
#include <score/document/DocumentInterface.hpp>
#include <vector>

#include "RefreshStates.hpp"
#include "RefreshStatesMacro.hpp"
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Process/State/MessageNode.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/selection/SelectionStack.hpp>

namespace Scenario
{
namespace Command
{

void RefreshStates(const score::DocumentContext& doc)
{
  using namespace std;
  // Fetch the selected intervals

  // TODO this method can also be used in IScoreCohesion's other algorithms.
  auto selected_states = filterSelectionByType<StateModel>(
      doc.selectionStack.currentSelection());

  RefreshStates(selected_states, doc.commandStack);
}

void RefreshStates(
    const QList<const StateModel*>& states,
    const score::CommandStackFacade& stack)
{
  if (states.empty())
    return;

  auto& doc = score::IDocument::documentContext(*states.first());
  auto& proxy = doc.plugin<Explorer::DeviceDocumentPlugin>().updateProxy;

  auto macro = new RefreshStatesMacro;

  for (auto st : states)
  {
    const auto& state = *st;
    auto messages = flatten(state.messages().rootNode());
    for (auto& elt : messages)
    {
      auto val = proxy.refreshRemoteValue(elt.address.address);
      SCORE_TODO; // FIXME merge the value with the address accessor
      elt.value = val;
    }
    macro->addCommand(new AddMessagesToState{state, messages});
  }

  CommandDispatcher<> disp{stack};
  disp.submitCommand(macro);
}
}
}
