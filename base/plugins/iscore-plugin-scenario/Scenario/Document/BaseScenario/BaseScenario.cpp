#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QString>
#include <tuple>

#include "BaseScenario.hpp"
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/std/StdlibWrapper.hpp>
#include <iscore/tools/std/Algorithms.hpp>

class QObject;

ISCORE_METADATA_IMPL(Scenario::BaseScenario)
namespace Scenario
{
BaseScenario::BaseScenario(const Id<BaseScenario>& id, QObject* parent):
    IdentifiedObject<BaseScenario>{id, "BaseScenario", parent},
    BaseScenarioContainer{this},
    pluginModelList{iscore::IDocument::documentContext(*parent), this}
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


const QVector<Id<ConstraintModel> > constraintsBeforeTimeNode(
        const BaseScenario& scen,
        const Id<TimeNodeModel>& timeNodeId)
{
    if(timeNodeId == scen.endTimeNode().id())
    {
        return {scen.constraint().id()};
    }
    return {};
}
}

template<>
QString NameInUndo<Scenario::BaseScenario>()
{
    return "BaseScenario";
}
