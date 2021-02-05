#pragma once
#include <Process/State/MessageNode.hpp>
#include <State/Message.hpp>

namespace Process
{
class ProcessModel;
}
#include <score/model/Identifier.hpp>

namespace Scenario
{
// An enum that says if a process is before or after the state.
enum class ProcessPosition
{
  Previous,
  Following
};

// User messages
void updateTreeWithMessageList(Process::MessageNode& rootNode, State::MessageList lst);
void renameAddress(Process::MessageNode& rootNode, const State::AddressAccessor& oldAddr, const State::AddressAccessor& newAddr);

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

void updateTreeWithRemovedInterval(Process::MessageNode& rootNode, ProcessPosition pos);

void updateTreeWithRemovedUserMessage(
    Process::MessageNode& rootNode,
    const State::AddressAccessor&);

void updateTreeWithRemovedNode(Process::MessageNode& rootNode, const State::AddressAccessor& addr);

void removeAllUserMessages(Process::MessageNode& rootNode);

int countNodes(Process::MessageNode& rootNode);
Process::MessageNode* getNthChild(Process::MessageNode& rootNode, int n);
int getChildIndex(Process::MessageNode& rootNode, Process::MessageNode* n);
}
