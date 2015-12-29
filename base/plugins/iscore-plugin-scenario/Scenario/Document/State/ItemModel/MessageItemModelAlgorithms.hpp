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
        MessageNode& rootNode,
        iscore::MessageList lst);

// Messages from a process
void updateTreeWithMessageList(
        MessageNode& rootNode,
        iscore::MessageList lst,
        const Id<Process::ProcessModel>& proc,
        ProcessPosition pos);

void updateTreeWithRemovedProcess(
        MessageNode& rootNode,
        const Id<Process::ProcessModel>& proc,
        ProcessPosition pos);

void updateTreeWithRemovedConstraint(
        MessageNode& rootNode,
        ProcessPosition pos);

void updateTreeWithRemovedUserMessage(
        MessageNode& rootNode,
        const iscore::Address&);

void updateTreeWithRemovedNode(
        MessageNode& rootNode,
        const iscore::Address& addr);
