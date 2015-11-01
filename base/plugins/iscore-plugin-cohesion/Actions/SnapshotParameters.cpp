#include "SnapshotParameters.hpp"

#include <Commands/CreateStatesFromParametersInEvents.hpp>

#include <Scenario/Commands/State/UpdateState.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

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
    auto device_explorer = doc->model().pluginModel<DeviceDocumentPlugin>()->updateProxy.deviceExplorer;

    iscore::MessageList messages = getSelectionSnapshot(*device_explorer);
    if(messages.empty())
        return;

    MacroCommandDispatcher macro{new SnapshotStatesMacro,
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
