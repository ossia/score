#include "BaseScenario.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

BaseScenario::BaseScenario(const Id<BaseScenario>& id, QObject* parent):
    IdentifiedObject<BaseScenario>{id, "BaseScenario", parent},
    BaseScenarioContainer{this},
    pluginModelList{iscore::IDocument::documentFromObject(parent), this}
{
    BaseScenarioContainer::init();

    m_endNode->trigger()->setActive(true);
}

Selection BaseScenario::selectedChildren() const
{
    Selection s;
    for_each_in_tuple(elements(), [&] (auto elt) {
        if(elt->selection.get())
            s.append(elt);
    });
    return s;
}

template<>
QString NameInUndo<BaseScenario>()
{
    return "BaseScenario";
}
