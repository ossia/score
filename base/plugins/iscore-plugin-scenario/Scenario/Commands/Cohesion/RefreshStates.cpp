#include "RefreshStates.hpp"

#include <Scenario/Process/ScenarioModel.hpp>

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include "RefreshStatesMacro.hpp"

void RefreshStates(iscore::Document* doc)
{
    using namespace std;
    // Fetch the selected constraints

    // TODO this method can also be used in IScoreCohesion's other algorithms.
    auto selected_states = filterSelectionByType<StateModel>(doc->selectionStack().currentSelection());

    RefreshStates(selected_states, doc->commandStack());
}

void RefreshStates(
        const QList<const StateModel*>& states,
        iscore::CommandStack& stack)
{
    if(states.empty())
        return;

    auto doc = iscore::IDocument::documentFromObject(*states.first());
    auto proxy = doc->model().pluginModel<DeviceDocumentPlugin>()->updateProxy;

    auto macro = new RefreshStatesMacro;

    for(auto st : states)
    {
        const auto& state = *st;
        auto messages = flatten(state.messages().rootNode());
        for(auto& elt : messages)
        {
            elt.value = proxy.refreshRemoteValue(elt.address);
        }
        macro->addCommand(new AddMessagesToState{state.messages(), messages});
    }

    CommandDispatcher<> disp{stack};
    disp.submitCommand(macro);
}
