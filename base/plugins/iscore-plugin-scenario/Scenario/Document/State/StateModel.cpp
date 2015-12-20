#include <Process/State/ProcessStateDataInterface.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QAbstractItemModel>
#include <QObject>
#include <algorithm>

#include "ItemModel/MessageItemModelAlgorithms.hpp"
#include <Process/Process.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <State/Message.hpp>
#include "StateModel.hpp"
#include <iscore/document/DocumentContext.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

/*
#include <Process/ProcessList.hpp>
#include <iscore/document/DocumentContext.hpp>
#include <iscore/application/ApplicationContext.hpp>

*/
constexpr const char StateModel::className[];
StateModel::StateModel(
        const Id<StateModel>& id,
        const Id<EventModel>& eventId,
        double yPos,
        iscore::CommandStackFacade& stack,
        QObject *parent):
    IdentifiedObject<StateModel> {id, "StateModel", parent},
    m_stack{stack},
    m_eventId{eventId},
    m_heightPercentage{yPos},
    m_messageItemModel{new MessageItemModel{
                            stack,
                            *this,
                            this}}
{
    init();
/*
    auto ctx = iscore::IDocument::documentContext(*this);
    auto& fact = *ctx.app.components.factory<StateProcessList>().list().get().begin()->second;
    stateProcesses.add(fact.make(Id<StateProcess>{123}, this));
    */
}

StateModel::StateModel(
        const StateModel &source,
        const Id<StateModel> &id,
        iscore::CommandStackFacade& stack,
        QObject *parent):
    IdentifiedObject<StateModel> {id, "StateModel", parent},
    m_stack{stack},
    m_eventId{source.eventId()},
    m_previousConstraint{source.previousConstraint()},
    m_nextConstraint{source.nextConstraint()},
    m_heightPercentage{source.heightPercentage()},
    m_messageItemModel{new MessageItemModel{
                            m_stack,
                            *this,
                            this}}
{
    // FIXME Source has to be in the same document else it will crash.
    // FIXME prune the messages from the prev / next processes data and rebuild it.
    messages() = source.messages();

    init();
}

void StateModel::init()
{
    con(m_messageItemModel, &QAbstractItemModel::modelReset,
        this, &StateModel::statesUpdated_slt);
    con(m_messageItemModel, &QAbstractItemModel::dataChanged,
        this, &StateModel::statesUpdated_slt);
    con(m_messageItemModel, &QAbstractItemModel::rowsInserted,
        this, &StateModel::statesUpdated_slt);
    con(m_messageItemModel, &QAbstractItemModel::rowsMoved,
        this, &StateModel::statesUpdated_slt);
    con(m_messageItemModel, &QAbstractItemModel::rowsRemoved,
        this, &StateModel::statesUpdated_slt);

    if(m_previousConstraint)
    {
        setPreviousConstraint(m_previousConstraint);
    }
    if(m_nextConstraint)
    {
        setNextConstraint(m_nextConstraint);
    }
}

double StateModel::heightPercentage() const
{
    return m_heightPercentage;
}

void StateModel::setHeightPercentage(double y)
{
    if(m_heightPercentage == y)
        return;
    m_heightPercentage = y;
    emit heightPercentageChanged();
}

void StateModel::statesUpdated_slt()
{
    emit sig_statesUpdated();
}

const Id<EventModel> &StateModel::eventId() const
{
    return m_eventId;
}

void StateModel::setEventId(const Id<EventModel> & id)
{
    m_eventId = id;
}

const Id<ConstraintModel> &StateModel::previousConstraint() const
{
    return m_previousConstraint;
}

const Id<ConstraintModel> &StateModel::nextConstraint() const
{
    return m_nextConstraint;
}

void StateModel::setNextConstraint(const Id<ConstraintModel> & id)
{
    m_nextConstraint = id;
}

void StateModel::setPreviousConstraint(const Id<ConstraintModel> & id)
{
    m_previousConstraint = id;
}


MessageItemModel& StateModel::messages() const
{
    return *m_messageItemModel;
}

void StateModel::setStatus(ExecutionStatus status)
{
    if (m_status.get() == status)
        return;

    m_status.set(status);
    emit statusChanged(status);
}
