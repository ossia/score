#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <algorithm>
#include <vector>

#include <Explorer/DocumentPlugin/NodeUpdateProxy.hpp>
#include <Process/State/MessageNode.hpp>
#include "RefreshStates.hpp"
#include "RefreshStatesMacro.hpp"
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>
#include <iscore/command/Dispatchers/CommandDispatcher.hpp>
#include <iscore/selection/SelectionStack.hpp>

namespace Scenario
{
namespace Command
{

void RefreshStates(const iscore::DocumentContext& doc)
{
    using namespace std;
    // Fetch the selected constraints

    // TODO this method can also be used in IScoreCohesion's other algorithms.
    auto selected_states = filterSelectionByType<StateModel>(doc.selectionStack.currentSelection());

    RefreshStates(selected_states, doc.commandStack);
}

void RefreshStates(
        const QList<const StateModel*>& states,
        iscore::CommandStackFacade& stack)
{
    if(states.empty())
        return;

    auto& doc = iscore::IDocument::documentContext(*states.first());
    auto& proxy = doc.plugin<Explorer::DeviceDocumentPlugin>().updateProxy;

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

}
}
