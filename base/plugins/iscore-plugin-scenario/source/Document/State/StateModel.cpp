#include "StateModel.hpp"

#include "StateView.hpp"

#include "Document/Constraint/ViewModels/ConstraintView.hpp"
#include "Document/Event/EventModel.hpp"

#include "Process/ScenarioModel.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include <ProcessInterface/State/ProcessStateDataInterface.hpp>
#include <iscore/document/DocumentInterface.hpp>

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
        const auto& cstr = parentScenario()->constraint(m_previousConstraint);
        for(const auto& proc : cstr.processes)
        {
            on_previousProcessAdded(proc);
        }
    }
    if(m_nextConstraint)
    {
        const auto& cstr = parentScenario()->constraint(m_nextConstraint);
        for(const auto& proc : cstr.processes)
        {
            on_nextProcessAdded(proc);
        }
    }
}

const ScenarioInterface* StateModel::parentScenario() const
{
    return dynamic_cast<ScenarioInterface*>(parent());
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

// TESTME
void StateModel::statesUpdated_slt()
{
    // Check for all the messages in the state, for some
    // that are matching our processes's addresses.
    // If they match, update them in the process state.
    // OPTIMIZEME

    ISCORE_TODO;

    /*
    auto ml = messages().flatten();
    for(const auto& proc : m_previousProcesses)
    {
        auto currentMessages = proc->messages();
        for(const auto& mess : ml)
        {
            // OPTIMIZEME with set_intersection
            auto it = std::find_if(
                currentMessages.begin(),
                currentMessages.end(),
                [&] (const auto& m) { return m.address == mess.address; });

            if(it != currentMessages.end())
            {
                it->value = mess.value;
            }
        }
        proc->setMessages(currentMessages);
    }
    // TODO refactor
    for(const auto& proc : m_nextProcesses)
    {
        auto currentMessages = proc->messages();
        for(const auto& mess : ml)
        {
            // OPTIMIZEME with set_intersection
            auto it = std::find_if(
                currentMessages.begin(),
                currentMessages.end(),
                [&] (const auto& m) { return m.address == mess.address; });

            if(it != currentMessages.end())
            {
                it->value = mess.value;
            }
        }
        proc->setMessages(currentMessages);
    }
    emit sig_statesUpdated();
    */
}

// When we add a process, the data in the process takes precedence on the states
// present here, because it might contain saved states at the beginning /
// end that we don't want to change.
void StateModel::on_previousProcessAdded(const Process& proc)
{
    ProcessStateDataInterface* state = proc.endState();
    if(!state)
        return;

    connect(state, &ProcessStateDataInterface::messagesChanged,
            this, [&] (const iscore::MessageList& ml) {
        // We merge the messages with the state.
        // No need to undo-redo this, this is an invariant.
        // TODO have some collapsing between all the processes of a state
        // NOTE how to prevent these states from being played
        // twice ? mark them ?
        // TODO which one shoul be sent ? the ones
        // from the process ?

        ISCORE_TODO;
        /*
        for(const auto& mess : ml)
        {
            messages().insert(mess);
        }
        */
    });

    m_previousProcesses.insert(state);
    statesUpdated_slt();
}

void StateModel::on_previousProcessRemoved(const Process& proc)
{
    ProcessStateDataInterface* state = proc.endState();
    if(!state)
        return;

    m_previousProcesses.erase(state);
}

void StateModel::on_nextProcessAdded(const Process& proc)
{
    ProcessStateDataInterface* state = proc.startState();
    if(!state)
        return;

    connect(state, &ProcessStateDataInterface::messagesChanged,
        this, [&] (const iscore::MessageList& ml) {

        ISCORE_TODO;
        /*
        for(const auto& mess : ml)
        {
            messages().insert(mess);
        }
        */
    });

    m_nextProcesses.insert(state);
    statesUpdated_slt();
}

void StateModel::on_nextProcessRemoved(const Process& proc)
{
    ProcessStateDataInterface* state = proc.startState();
    if(!state)
        return;

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
    m_nextConstraint = id;

    if(!m_nextConstraint)
        return;

    auto& cstr = parentScenario()->constraint(m_nextConstraint);
    con(cstr.processes, &NotifyingMap<Process>::added,
        this, &StateModel::on_nextProcessAdded);
    con(cstr.processes, &NotifyingMap<Process>::removed,
        this, &StateModel::on_nextProcessRemoved);
}

void StateModel::setPreviousConstraint(const Id<ConstraintModel> & id)
{
    m_previousConstraint = id;

    if(!m_previousConstraint)
        return;

    auto& cstr = parentScenario()->constraint(m_previousConstraint);
    con(cstr.processes, &NotifyingMap<Process>::added,
        this, &StateModel::on_previousProcessAdded);
    con(cstr.processes, &NotifyingMap<Process>::removed,
        this, &StateModel::on_previousProcessRemoved);
}


const MessageItemModel& StateModel::messages() const
{
    return *m_messageItemModel;
}

MessageItemModel& StateModel::messages()
{
    return *m_messageItemModel;
}


void StateModel::setStatus(EventStatus status)
{
    if (m_status == status)
        return;

    m_status = status;
    emit statusChanged(status);
}
