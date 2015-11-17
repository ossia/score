#include "BaseScenario.hpp"
#include <iscore/document/DocumentInterface.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

class ConstraintViewModel;

BaseScenario::BaseScenario(const Id<BaseScenario>& id, QObject* parent):
    IdentifiedObject<BaseScenario>{id, "BaseScenario", parent},
    BaseScenarioContainer{this},
    pluginModelList{iscore::IDocument::documentFromObject(parent), this}
{
    BaseScenarioContainer::init();

    m_endNode->trigger()->setActive(true);
}
