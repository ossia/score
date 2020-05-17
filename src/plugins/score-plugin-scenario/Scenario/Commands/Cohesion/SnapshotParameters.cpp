// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SnapshotParameters.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Commands/State/SnapshotStatesMacro.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <State/Message.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/IdentifiedObjectAbstract.hpp>
#include <score/model/path/Path.hpp>
#include <score/selection/Selectable.hpp>
#include <score/selection/SelectionStack.hpp>

#include <QList>

#include <vector>

namespace Scenario
{
void SnapshotParametersInStates(const score::DocumentContext& doc)
{
  using namespace std;
  // Fetch the selected events
  auto sel = doc.selectionStack.currentSelection();

  QList<const Scenario::StateModel*> selected_states;
  for (auto obj : sel)
  {
    if (auto st = dynamic_cast<const Scenario::StateModel*>(obj.data()))
      if (st->selection.get()) // TODO this should not be necessary?
        selected_states.push_back(st);
  }

  // Fetch the selected DeviceExplorer elements
  State::MessageList messages
      = Explorer::getSelectionSnapshot(Explorer::deviceExplorerFromContext(doc));
  if (messages.empty())
    return;

  MacroCommandDispatcher<SnapshotStatesMacro> macro{doc.commandStack};
  for (auto& state : selected_states)
  {
    auto cmd = new Scenario::Command::AddMessagesToState{*state, messages};
    macro.submit(cmd);
  }

  macro.commit();
}
}
