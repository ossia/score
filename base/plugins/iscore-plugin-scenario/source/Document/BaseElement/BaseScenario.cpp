#include "BaseScenario.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/TimeNode/TimeNodeModel.hpp"
#include "source/Document/Constraint/ConstraintModel.hpp"
#include <iscore/document/DocumentInterface.hpp>

class AbstractConstraintViewModel;
BaseScenario::BaseScenario(const id_type<BaseScenario>& id, QObject* parent):
    IdentifiedObject<BaseScenario>{id, "BaseScenario", parent},
    pluginModelList{iscore::IDocument::documentFromObject(parent), this},
    m_startNode{new TimeNodeModel{id_type<TimeNodeModel>{0}, TimeValue::zero(), 0, this}},
    m_endNode{new TimeNodeModel{id_type<TimeNodeModel>{1}, TimeValue::zero(), 0, this}}, // TODO baseconstraint duration
    m_startEvent{new EventModel{id_type<EventModel>{0}, m_startNode->id(), 0, this}},
    m_endEvent{new EventModel{id_type<EventModel>{1}, m_endNode->id(), 0, this}},
    m_constraint {new ConstraintModel{
                            id_type<ConstraintModel>{0},
                            id_type<AbstractConstraintViewModel>{0},
                            0,
                            this}}
{
    qDebug() << "TODO: " << Q_FUNC_INFO;
    /*
    m_startNode->setObjectName("BaseStartTimeNodeModel");
    m_startEvent->setObjectName("BaseStartEventModel");
    m_startNode->addEvent(m_startEvent->id());

    m_endNode->setObjectName("BaseEndTimeNodeModel");
    m_endEvent->setObjectName("BaseEndEventModel");
    m_endNode->addEvent(m_endEvent->id());

    m_constraint->setStartEvent(m_startEvent->id());
    m_constraint->setEndEvent(m_endEvent->id());

    ConstraintModel::Algorithms::changeAllDurations(*m_constraint, std::chrono::minutes{3});
    m_constraint->setObjectName("BaseConstraintModel");
    */
}


ConstraintModel*BaseScenario::baseConstraint() const
{
    return m_constraint;
}

TimeNodeModel *BaseScenario::startTimeNode() const
{
    return m_startNode;
}

TimeNodeModel *BaseScenario::endTimeNode() const
{
    return m_endNode;
}

EventModel *BaseScenario::startEvent() const
{
    return m_startEvent;
}

EventModel *BaseScenario::endEvent() const
{
    return m_endEvent;
}
