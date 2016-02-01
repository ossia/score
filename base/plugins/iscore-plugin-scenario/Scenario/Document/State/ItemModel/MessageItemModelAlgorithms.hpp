#pragma once
#include <Process/State/MessageNode.hpp>
#include <State/Message.hpp>

namespace Process { class ProcessModel; }
#include <iscore/tools/SettableIdentifier.hpp>

// An enum that says if a process is before or after the state.
enum class ProcessPosition {
    Previous, Following
};

// User messages
void updateTreeWithMessageList(
        Process::MessageNode& rootNode,
        State::MessageList lst);

// Messages from a process
void updateTreeWithMessageList(
        Process::MessageNode& rootNode,
        State::MessageList lst,
        const Id<Process::ProcessModel>& proc,
        ProcessPosition pos);

void updateTreeWithRemovedProcess(
        Process::MessageNode& rootNode,
        const Id<Process::ProcessModel>& proc,
        ProcessPosition pos);

void updateTreeWithRemovedConstraint(
        Process::MessageNode& rootNode,
        ProcessPosition pos);

void updateTreeWithRemovedUserMessage(
        Process::MessageNode& rootNode,
        const State::Address&);

void updateTreeWithRemovedNode(
        Process::MessageNode& rootNode,
        const State::Address& addr);
