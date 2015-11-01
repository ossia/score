#pragma once
#include <State/Message.hpp>
#include <Process/State/MessageNode.hpp>
class Process;
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
        const Id<Process>& proc,
        ProcessPosition pos);

void updateTreeWithRemovedProcess(
        MessageNode& rootNode,
        const Id<Process>& proc,
        ProcessPosition pos);

void updateTreeWithRemovedConstraint(
        MessageNode& rootNode,
        ProcessPosition pos);

void updateTreeWithRemovedUserMessage(
        MessageNode& rootNode,
        const iscore::Message&);

