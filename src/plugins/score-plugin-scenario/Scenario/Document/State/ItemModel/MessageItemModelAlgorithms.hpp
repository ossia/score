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


#include <Scenario/Document/State/ItemModel/MessageItemModel.hpp>
namespace Scenario
{
// User messages
inline
void updateModelWithMessageList(MessageItemModel& model, State::MessageList lst)
{
  model.beginResetModel();
  updateTreeWithMessageList(model.rootNode(), std::move(lst));
  model.endResetModel();
}

inline
void renameAddress(MessageItemModel& model, const State::AddressAccessor& oldAddr, const State::AddressAccessor& newAddr)
{
  model.beginResetModel();
  renameAddress(model.rootNode(), oldAddr, newAddr);
  model.endResetModel();
}

// Messages from a process
inline
void updateModelWithMessageList(
    MessageItemModel& model,
    State::MessageList lst,
    const Id<Process::ProcessModel>& proc,
    ProcessPosition pos)
{
  model.beginResetModel();
  updateTreeWithMessageList(model.rootNode(), std::move(lst), proc, pos);
  model.endResetModel();
}

inline
void updateModelWithRemovedProcess(
    MessageItemModel& model,
    const Id<Process::ProcessModel>& proc,
    ProcessPosition pos)
{
  model.beginResetModel();
  updateTreeWithRemovedProcess(model.rootNode(), proc, pos);
  model.endResetModel();
}

inline
void updateModelWithRemovedInterval(MessageItemModel& model, ProcessPosition pos)
{
  model.beginResetModel();
  updateTreeWithRemovedInterval(model.rootNode(), pos);
  model.endResetModel();
}

inline
void updateModelWithRemovedUserMessage(
    MessageItemModel& model,
    const State::AddressAccessor& addr)
{
  model.beginResetModel();
  updateTreeWithRemovedUserMessage(model.rootNode(), addr);
  model.endResetModel();
}

inline
void updateModelWithRemovedNode(MessageItemModel& model, const State::AddressAccessor& addr)
{
  model.beginResetModel();
  updateTreeWithRemovedNode(model.rootNode(), addr);
  model.endResetModel();
}

inline
void removeAllUserMessages(MessageItemModel& model)
{
  model.beginResetModel();
  removeAllUserMessages(model.rootNode());
  model.endResetModel();
}
}
