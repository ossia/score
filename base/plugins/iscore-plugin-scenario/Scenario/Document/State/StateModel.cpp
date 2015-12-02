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

        auto cstr = dynamic_cast<ScenarioInterface*>(parent())->findConstraint(m_nextConstraint);
        if(cstr)
        {
            cstr->processes.added.disconnect<StateModel,&StateModel::on_nextProcessAdded>(this);
            cstr->processes.removed.disconnect<StateModel,&StateModel::on_nextProcessRemoved>(this);
        }

        m_nextProcesses.clear();
    }

    m_nextConstraint = id;

    if(!m_nextConstraint)
        return;

    auto scenar = dynamic_cast<ScenarioInterface*>(parent());
    if(scenar)
    {
        // The constraints might not be present in a scenario
        // for instance when restoring removed elements.
        auto cstr = scenar->findConstraint(m_nextConstraint);
        if(cstr)
        {
            for(const auto& proc : cstr->processes)
            {
                on_nextProcessAdded(proc);
            }

            cstr->processes.added.connect<StateModel,&StateModel::on_nextProcessAdded>(this);
            cstr->processes.removed.connect<StateModel,&StateModel::on_nextProcessRemoved>(this);
        }
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

        auto cstr = dynamic_cast<ScenarioInterface*>(parent())->findConstraint(m_previousConstraint);
        if(cstr)
        {
            cstr->processes.added.disconnect<StateModel,&StateModel::on_previousProcessAdded>(this);
            cstr->processes.removed.disconnect<StateModel,&StateModel::on_previousProcessRemoved>(this);
        }

        m_previousProcesses.clear();
    }

    m_previousConstraint = id;

    if(!m_previousConstraint)
        return;

    auto scenar = dynamic_cast<ScenarioInterface*>(parent());
    if(scenar)
    {
        auto cstr = scenar->findConstraint(m_previousConstraint);
        if(cstr)
        {
            for(const auto& proc : cstr->processes)
            {
                on_previousProcessAdded(proc);
            }

            cstr->processes.added.connect<StateModel,&StateModel::on_previousProcessAdded>(this);
            cstr->processes.removed.connect<StateModel,&StateModel::on_previousProcessRemoved>(this);
        }
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
