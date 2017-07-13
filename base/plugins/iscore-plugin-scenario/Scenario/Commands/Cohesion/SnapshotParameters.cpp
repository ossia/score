// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <QList>
#include <QPointer>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Commands/State/SnapshotStatesMacro.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <algorithm>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <vector>

#include "SnapshotParameters.hpp"
#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <State/Message.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/model/IdentifiedObjectAbstract.hpp>
#include <iscore/model/path/Path.hpp>

namespace Scenario
{
void SnapshotParametersInStates(const iscore::DocumentContext& doc)
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
  State::MessageList messages = Explorer::getSelectionSnapshot(
      Explorer::deviceExplorerFromContext(doc));
  if (messages.empty())
    return;

  MacroCommandDispatcher<SnapshotStatesMacro> macro{doc.commandStack};
  for (auto& state : selected_states)
  {
    auto cmd = new Scenario::Command::AddMessagesToState{*state, messages};
    macro.submitCommand(cmd);
  }

  macro.commit();
}
}
