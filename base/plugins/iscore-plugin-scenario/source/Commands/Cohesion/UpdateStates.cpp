#include "UpdateStates.hpp"

#include <Process/ScenarioModel.hpp>

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Plugin/Panel/DeviceExplorerModel.hpp>
#include <Commands/State/UpdateState.hpp>
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
        auto messages = state.messages().flatten();
        for(auto& elt : messages)
        {
            if(auto val = proxy.refreshRemoteValue(elt.address))
            {
                elt.value = *val;
            }
        }
        macro->addCommand(new AddMessagesToState{state.messages(), messages});
    }

    CommandDispatcher<> disp{stack};
    disp.submitCommand(macro);
}
