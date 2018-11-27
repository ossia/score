#pragma once
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>

#include <score/model/Identifier.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreePath.hpp>
#include <score/tools/std/Optional.hpp>

#include <QString>
#include <QStringList>

#include <score_lib_process_export.h>

#include <array>

namespace Process
{
class ProcessModel;
struct SCORE_LIB_PROCESS_EXPORT StateNodeValues
{
  // TODO use lists or queues instead to manage the priorities
  struct QualifiedValue
  {
    State::DestinationQualifiers qualifiers;
    ossia::value value;
    friend bool operator==(const QualifiedValue& lhs, const QualifiedValue& rhs) noexcept
    { return lhs.qualifiers == rhs.qualifiers && lhs.value == rhs.value; }
  };

  std::vector<QualifiedValue> userValue;

  bool hasValue() const noexcept;
  // TODO here we have to choose a policy
  // if we have both previous and following processes ?
  optional<ossia::value> value() const noexcept;
  ossia::value filledValue(int n) const noexcept;

  friend bool operator==(const StateNodeValues& lhs, const StateNodeValues& rhs) noexcept;
  bool empty() const noexcept;

  ossia::unit_t unit() const noexcept;
};

struct SCORE_LIB_PROCESS_EXPORT StateNodeData
{
  QString name;
  StateNodeValues values;

  QString displayName() const;
  bool hasValue() const;
  optional<ossia::value> value() const;
};

SCORE_LIB_PROCESS_EXPORT QDebug
operator<<(QDebug d, const StateNodeData& mess);

using MessageNode = TreeNode<StateNodeData>;
using MessageNodePath = TreePath<MessageNode>;

SCORE_LIB_PROCESS_EXPORT State::AddressAccessor
address(const MessageNode& treeNode);
SCORE_LIB_PROCESS_EXPORT State::Message message(const MessageNode& node);
SCORE_LIB_PROCESS_EXPORT State::Message userMessage(const MessageNode& node);

SCORE_LIB_PROCESS_EXPORT Process::MessageNode* try_getNodeFromAddress(
    Process::MessageNode& root, const State::AddressAccessor& addr);
SCORE_LIB_PROCESS_EXPORT std::vector<Process::MessageNode*>
try_getNodesFromAddress(
    Process::MessageNode& root, const State::AddressAccessor& addr);
SCORE_LIB_PROCESS_EXPORT State::MessageList flatten(const MessageNode&);

SCORE_LIB_PROCESS_EXPORT
std::vector<const State::Message*> try_getNodesFromAddress(
    const State::MessageList& root, const State::AddressAccessor& addr);
}

extern template class SCORE_LIB_PROCESS_EXPORT
    TreeNode<Process::StateNodeData>;
