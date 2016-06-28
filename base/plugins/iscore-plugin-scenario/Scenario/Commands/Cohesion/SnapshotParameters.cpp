#include <Commands/SnapshotStatesMacro.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <QList>
#include <QPointer>
#include <algorithm>
#include <vector>

#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include "SnapshotParameters.hpp"
#include <State/Message.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/selection/SelectionStack.hpp>
#include <iscore/tools/IdentifiedObjectAbstract.hpp>
#include <iscore/tools/ModelPath.hpp>

void SnapshotParametersInStates(const iscore::DocumentContext& doc)
{
    using namespace std;
    // Fetch the selected events
    auto sel = doc.
            selectionStack.
            currentSelection();

    QList<const Scenario::StateModel*> selected_states;
    for(auto obj : sel)
    {
        if(auto st = dynamic_cast<const Scenario::StateModel*>(obj.data()))
            if(st->selection.get()) // TODO this should not be necessary?
                selected_states.push_back(st);
    }

    // Fetch the selected DeviceExplorer elements
    State::MessageList messages = Explorer::getSelectionSnapshot(Explorer::deviceExplorerFromContext(doc));
    if(messages.empty())
        return;

    MacroCommandDispatcher macro{new SnapshotStatesMacro,
                doc.commandStack};
    for(auto& state : selected_states)
    {
        auto cmd = new Scenario::Command::AddMessagesToState{
                state->messages(),
                messages};
        macro.submitCommand(cmd);
    }

    macro.commit();
}
