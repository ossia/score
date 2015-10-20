#include "SnapshotParameters.hpp"

#include <Commands/CreateStatesFromParametersInEvents.hpp>

#include <Commands/State/UpdateState.hpp>
#include <Document/State/StateModel.hpp>
#include <Plugin/Panel/DeviceExplorerModel.hpp>
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Singletons/DeviceExplorerInterface.hpp>

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <core/document/Document.hpp>

void SnapshotParametersInStates(iscore::Document* doc)
{
    using namespace std;
    // Fetch the selected events
    auto sel = doc->
            selectionStack().
            currentSelection();

    QList<const StateModel*> selected_states;
    for(auto obj : sel)
    {
        if(auto st = dynamic_cast<const StateModel*>(obj.data()))
            if(st->selection.get()) // TODO this should not be necessary?
                selected_states.push_back(st);
    }

    // Fetch the selected DeviceExplorer elements
    auto device_explorer = doc->findChild<DeviceExplorerModel*>("DeviceExplorerModel");
    auto uniqueNodes = device_explorer->uniqueSelectedNodes(device_explorer->selectedIndexes());
    device_explorer->deviceModel().updateProxy.refreshRemoteValues(uniqueNodes);

    iscore::MessageList messages;
    for(const auto& node : uniqueNodes)
    {
        iscore::messageList(*node, messages);
    }

    if(messages.empty())
        return;

    MacroCommandDispatcher macro{new CreateStatesFromParametersInEvents,
                doc->commandStack()};
    for(auto& state : selected_states)
    {
        auto cmd = new AddMessagesToState{
                state->messages(),
                messages};
        macro.submitCommand(cmd);
    }

    macro.commit();
}
