#pragma once
#include <State/Address.hpp>
#include <State/Message.hpp>
#include <State/Value.hpp>

#include <score/model/Identifier.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/TreePath.hpp>
#include <score/tools/std/Optional.hpp>

#include <QString>
#include <QVector>

#include <score_lib_process_export.h>

#include <array>

namespace Process
{
class ProcessModel;

struct ProcessStateData
{
  Id<Process::ProcessModel> process;
  State::OptionalValue value;
};

enum class PriorityPolicy
{
  User,
  Previous,
  Following
};

struct SCORE_LIB_PROCESS_EXPORT StateNodeValues
{
  bool empty() const;

  // TODO use lists or queues instead to manage the priorities
  QVector<ProcessStateData> previousProcessValues;
  QVector<ProcessStateData> followingProcessValues;
  State::OptionalValue userValue;

  std::array<PriorityPolicy, 3> priorities{{PriorityPolicy::Previous,
                                            PriorityPolicy::Following,
                                            PriorityPolicy::User}};

  bool hasValue() const;

  static bool hasValue(const QVector<ProcessStateData>& vec);
  static QVector<ProcessStateData>::const_iterator
  value(const QVector<ProcessStateData>& vec);

  // TODO here we have to choose a policy
  // if we have both previous and following processes ?
  State::OptionalValue value() const;

  QString displayValue() const;
};

struct SCORE_LIB_PROCESS_EXPORT StateNodeData
{
  State::AddressAccessorHead name;
  StateNodeValues values;

  QString displayName() const;
  bool hasValue() const;
  State::OptionalValue value() const;
};

SCORE_LIB_PROCESS_EXPORT QDebug
operator<<(QDebug d, const ProcessStateData& mess);
SCORE_LIB_PROCESS_EXPORT QDebug
operator<<(QDebug d, const StateNodeData& mess);

using MessageNode = TreeNode<StateNodeData>;
using MessageNodePath = TreePath<MessageNode>;

SCORE_LIB_PROCESS_EXPORT State::AddressAccessor
address(const MessageNode& treeNode);
SCORE_LIB_PROCESS_EXPORT State::Message message(const MessageNode& node);
SCORE_LIB_PROCESS_EXPORT State::Message userMessage(const MessageNode& node);

SCORE_LIB_PROCESS_EXPORT Process::MessageNode* try_getNodeFromAddress(
    Process::MessageNode& root,
    const State::AddressAccessor& addr);
SCORE_LIB_PROCESS_EXPORT std::vector<Process::MessageNode*>
try_getNodesFromAddress(
    Process::MessageNode& root,
    const State::AddressAccessor& addr);
SCORE_LIB_PROCESS_EXPORT State::MessageList flatten(const MessageNode&);
SCORE_LIB_PROCESS_EXPORT State::MessageList
getUserMessages(const MessageNode&);
}

#if !defined(SCORE_ALL_UNITY) && !defined(__MINGW32__)
extern template class SCORE_LIB_PROCESS_EXPORT
    TreeNode<Process::StateNodeData>;
#endif
