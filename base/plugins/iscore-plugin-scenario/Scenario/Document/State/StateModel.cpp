#include <Process/State/ProcessStateDataInterface.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <QAbstractItemModel>
#include <QObject>
#include <algorithm>

#include "ItemModel/MessageItemModelAlgorithms.hpp"
#include <Process/Process.hpp>
#include "Scenario/Document/Event/ExecutionStatus.hpp"
#include "Scenario/Document/State/ItemModel/MessageItemModel.hpp"
#include <State/Message.hpp>
#include "StateModel.hpp"
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>

constexpr const char StateModel::className[];
StateModel::StateModel(
        const Id<StateModel>& id,
        const Id<EventModel>& eventId,
        double yPos,
        QObject *parent):
    IdentifiedObject<StateModel> {id, "StateModel", parent},
    m_eventId{eventId},
    m_heightPercentage{yPos},
    m_messageItemModel{new MessageItemModel{
                            iscore::IDocument::commandStack(*this),
                            *this,
                            this}}
{
    init();
}

StateModel::StateModel(
        const StateModel &source,
        const Id<StateModel> &id,
        QObject *parent):
    StateModel{id, source.eventId(), source.heightPercentage(), parent}
{
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

void StateModel::on_previousProcessAdded(const Process& proc)
{
    ProcessStateDataInterface* state = proc.endStateData();
    if(!state)
        return;

    auto prev_proc_fun = [&] (const iscore::MessageList& ml) {
        // TODO have some collapsing between all the processes of a state
        // NOTE how to prevent these states from being played
        // twice ? mark them ?
        // TODO which one should be sent ? the ones
        // from the process ?

        auto node = m_messageItemModel->rootNode();
        updateTreeWithMessageList(node, ml, proc.id(), ProcessPosition::Previous);
        *m_messageItemModel = std::move(node);

        for(auto& next_proc : m_nextProcesses)
        {
            next_proc->setMessages(ml, m_messageItemModel->rootNode());
        }
    };
    connect(state, &ProcessStateDataInterface::messagesChanged,
            this, prev_proc_fun);

    prev_proc_fun(state->messages());

    m_previousProcesses.insert(state);
    statesUpdated_slt();
}

void StateModel::on_previousProcessRemoved(const Process& proc)
{
    ProcessStateDataInterface* state = proc.endStateData();
    if(!state)
        return;

    auto node = m_messageItemModel->rootNode();
    updateTreeWithRemovedProcess(node, proc.id(), ProcessPosition::Previous);
    *m_messageItemModel = std::move(node);

    m_previousProcesses.erase(state);
}

void StateModel::on_nextProcessAdded(const Process& proc)
{
    ProcessStateDataInterface* state = proc.startStateData();
    if(!state)
        return;

    auto next_proc_fun = [&] (const iscore::MessageList& ml) {

        auto node = m_messageItemModel->rootNode();
        updateTreeWithMessageList(node, ml, proc.id(), ProcessPosition::Following);
        *m_messageItemModel = std::move(node);

        // TODO if(synchronize) ...
        for(auto& prev_proc : m_previousProcesses)
        {
            prev_proc->setMessages(ml, m_messageItemModel->rootNode());
        }
    };

    connect(state, &ProcessStateDataInterface::messagesChanged,
            this, next_proc_fun);

    next_proc_fun(state->messages());

    m_nextProcesses.insert(state);
    statesUpdated_slt();
}

void StateModel::on_nextProcessRemoved(const Process& proc)
{
    ProcessStateDataInterface* state = proc.startStateData();
    if(!state)
        return;

    auto node = m_messageItemModel->rootNode();
    updateTreeWithRemovedProcess(node, proc.id(), ProcessPosition::Following);
    *m_messageItemModel = std::move(node);

    m_nextProcesses.erase(state);
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
    if(m_nextConstraint)
    {
        auto node = m_messageItemModel->rootNode();
        updateTreeWithRemovedConstraint(node, ProcessPosition::Following);
        *m_messageItemModel = std::move(node);

        for(auto conn : m_nextConnections)
            QObject::disconnect(conn);

        m_nextConnections.clear();
        m_nextProcesses.clear();
    }

    m_nextConstraint = id;

    if(!m_nextConstraint)
        return;

    auto scenar = dynamic_cast<ScenarioInterface*>(parent());
    ISCORE_ASSERT(scenar);

    // The constraints might not be present in a scenario
    // for instance when restoring removed elements.
    auto cstr = scenar->findConstraint(m_nextConstraint);
    if(cstr)
    {
        for(const auto& proc : cstr->processes)
        {
            on_nextProcessAdded(proc);
        }

        m_nextConnections.push_back(con(cstr->processes, &NotifyingMap<Process>::added,
            this, &StateModel::on_nextProcessAdded));
        m_nextConnections.push_back(con(cstr->processes, &NotifyingMap<Process>::removed,
            this, &StateModel::on_nextProcessRemoved));
    }
}

void StateModel::setPreviousConstraint(const Id<ConstraintModel> & id)
{
    // First clean the state model of the previous constraint's states
    if(m_previousConstraint)
    {
        auto node = m_messageItemModel->rootNode();
        updateTreeWithRemovedConstraint(node, ProcessPosition::Previous);
        *m_messageItemModel = std::move(node);

        for(auto conn : m_prevConnections)
            QObject::disconnect(conn);

        m_prevConnections.clear();
        m_previousProcesses.clear();
    }

    m_previousConstraint = id;

    if(!m_previousConstraint)
        return;

    auto scenar = dynamic_cast<ScenarioInterface*>(parent());
    ISCORE_ASSERT(scenar);

    auto cstr = scenar->findConstraint(m_previousConstraint);
    if(cstr)
    {
        for(const auto& proc : cstr->processes)
        {
            on_previousProcessAdded(proc);
        }

        m_prevConnections.push_back(con(cstr->processes, &NotifyingMap<Process>::added,
            this, &StateModel::on_previousProcessAdded));
        m_prevConnections.push_back(con(cstr->processes, &NotifyingMap<Process>::removed,
            this, &StateModel::on_previousProcessRemoved));
    }
}


MessageItemModel& StateModel::messages() const
{
    return *m_messageItemModel;
}

void StateModel::setStatus(ExecutionStatus status)
{
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged(status);
}
