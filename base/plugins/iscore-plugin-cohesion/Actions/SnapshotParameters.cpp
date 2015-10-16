#include "SnapshotParameters.hpp"

#include <Commands/CreateStatesFromParametersInEvents.hpp>

#include <Commands/State/UpdateState.hpp>
#include <Document/State/StateModel.hpp>
#include <Plugin/Panel/DeviceExplorerModel.hpp>
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
    auto indexes = device_explorer->selectedIndexes();

    iscore::MessageList messages;
    for(auto& index : indexes)
    {
        auto m = DeviceExplorer::messageFromModelIndex(index);
        if(m != iscore::Message{})
            messages.push_back(m);
    }

    if(messages.empty())
        return;

    MacroCommandDispatcher macro{new CreateStatesFromParametersInEvents,
                doc->commandStack()};
    for(auto& state : selected_states)
    {
        auto cmd = new UpdateState{
                state->messages(),
                messages};
        macro.submitCommand(cmd);
    }

    macro.commit();
}
