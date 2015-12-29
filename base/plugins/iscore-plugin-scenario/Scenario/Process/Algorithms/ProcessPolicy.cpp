#include "ProcessPolicy.hpp"
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
#include <Scenario/Document/State/ItemModel/MessageItemModelAlgorithms.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <Scenario/Process/Algorithms/Accessors.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <iscore/tools/std/Algorithms.hpp>

static void AddProcessBeforeState(
        StateModel& statemodel,
        const Process::ProcessModel& proc)
{
    // TODO this should be fused with the notion of State Process.
    ProcessStateDataInterface* state = proc.endStateData();
    if(!state)
        return;


    auto prev_proc_fun = [&] (const iscore::MessageList& ml) {
        // TODO have some collapsing between all the processes of a state
        // NOTE how to prevent these states from being played
        // twice ? mark them ?
        // TODO which one should be sent ? the ones
        // from the process ?
        auto& messages = statemodel.messages();

        auto node = messages.rootNode();
        updateTreeWithMessageList(node, ml, proc.id(), ProcessPosition::Previous);
        messages = std::move(node);

        for(const auto& next_proc : statemodel.followingProcesses())
        {
            next_proc.process().setMessages(ml, messages.rootNode());
        }
    };

    statemodel.previousProcesses().emplace_back(state);
    auto& wrapper = statemodel.previousProcesses().back();
    QObject::connect(state, &ProcessStateDataInterface::messagesChanged,
                     &wrapper, prev_proc_fun);

    prev_proc_fun(state->messages());

    emit statemodel.sig_statesUpdated();
}

static void AddProcessAfterState(StateModel& statemodel, const Process::ProcessModel& proc)
{
    ProcessStateDataInterface* state = proc.startStateData();
    if(!state)
        return;

    auto next_proc_fun = [&] (const iscore::MessageList& ml) {
        auto& messages = statemodel.messages();

        auto node = messages.rootNode();
        updateTreeWithMessageList(node, ml, proc.id(), ProcessPosition::Following);
        messages = std::move(node);

        for(const auto& prev_proc : statemodel.previousProcesses())
        {
            prev_proc.process().setMessages(ml, messages.rootNode());
        }
    };

    statemodel.followingProcesses().emplace_back(state);
    auto& wrapper = statemodel.followingProcesses().back();
    QObject::connect(state, &ProcessStateDataInterface::messagesChanged,
                     &wrapper, next_proc_fun);

    next_proc_fun(state->messages());

    emit statemodel.sig_statesUpdated();

}

static void RemoveProcessBeforeState(StateModel& statemodel, const Process::ProcessModel& proc)
{
    ProcessStateDataInterface* state = proc.endStateData();
    if(!state)
        return;

    auto node = statemodel.messages().rootNode();
    updateTreeWithRemovedProcess(node, proc.id(), ProcessPosition::Previous);
    statemodel.messages() = std::move(node);

    auto it = find_if(statemodel.previousProcesses(), [&] (const auto& elt) {
        return state->id() == elt.process().id();
    });

    statemodel.previousProcesses().erase(it);
}

static void RemoveProcessAfterState(StateModel& statemodel, const Process::ProcessModel& proc)
{
    ProcessStateDataInterface* state = proc.startStateData();
    if(!state)
        return;

    auto node = statemodel.messages().rootNode();
    updateTreeWithRemovedProcess(node, proc.id(), ProcessPosition::Following);
    statemodel.messages() = std::move(node);

    auto it = find_if(statemodel.followingProcesses(), [&] (const auto& elt) {
        return state->id() == elt.process().id();
    });

    statemodel.followingProcesses().erase(it);
}

void AddProcess(ConstraintModel& constraint, Process::ProcessModel* proc)
{
    constraint.processes.add(proc);

    const auto& scenar = *dynamic_cast<ScenarioInterface*>(constraint.parent());
    AddProcessAfterState(startState(constraint, scenar), *proc);
    AddProcessBeforeState(endState(constraint, scenar), *proc);
}

void RemoveProcess(ConstraintModel& constraint, const Id<Process::ProcessModel>& proc_id)
{
    const auto& proc = constraint.processes.at(proc_id);
    const auto& scenar = *dynamic_cast<ScenarioInterface*>(constraint.parent());

    RemoveProcessAfterState(startState(constraint, scenar), proc);
    RemoveProcessBeforeState(endState(constraint, scenar), proc);

    constraint.processes.remove(proc_id);
}

void SetPreviousConstraint(StateModel& state, const ConstraintModel& constraint)
{
    SetNoPreviousConstraint(state);

    state.setPreviousConstraint(constraint.id());
    for(const auto& proc : constraint.processes)
    {
        AddProcessBeforeState(state, proc);
    }
}

void SetNextConstraint(StateModel& state, const ConstraintModel& constraint)
{
    SetNoNextConstraint(state);

    state.setNextConstraint(constraint.id());
    for(const auto& proc : constraint.processes)
    {
        AddProcessAfterState(state, proc);
    }
}

void SetNoPreviousConstraint(StateModel& state)
{
    if(state.previousConstraint())
    {
        auto node = state.messages().rootNode();
        updateTreeWithRemovedConstraint(node, ProcessPosition::Previous);
        state.messages() = std::move(node);

        state.previousProcesses().clear();
        state.setPreviousConstraint(Id<ConstraintModel>{});
    }
}

void SetNoNextConstraint(StateModel& state)
{
    if(state.nextConstraint())
    {
        auto node = state.messages().rootNode();
        updateTreeWithRemovedConstraint(node, ProcessPosition::Following);
        state.messages() = std::move(node);

        state.followingProcesses().clear();
        state.setNextConstraint(Id<ConstraintModel>{});
    }
}
