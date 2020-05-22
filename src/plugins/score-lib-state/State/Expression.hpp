#pragma once
#include <State/Relation.hpp>

#include <score/model/tree/InvisibleRootNode.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/model/tree/VariantBasedNode.hpp>
#include <score/tools/std/Optional.hpp>

#include <ossia/editor/expression/operators.hpp>

#include <QString>

#include <cstddef>
#include <vector>

class DataStream;
class JSONObject;
template <typename DataType>
class TreeNode;

namespace State
{
using BinaryOperator = ossia::expressions::binary_operator;

enum class UnaryOperator
{
  Not
};

struct SCORE_LIB_STATE_EXPORT ExprData
    : public score::VariantBasedNode<Relation, Pulse, BinaryOperator, UnaryOperator>
{
  // SCORE_SERIALIZE_FRIENDS

  ExprData() = default;
  ExprData(const ExprData&) = default;
  ExprData(ExprData&&) = default;
  ExprData& operator=(const ExprData&) = default;
  ExprData& operator=(ExprData&&) = default;
  ExprData(Relation data) : VariantBasedNode{std::move(data)} { }

  ExprData(Pulse data) : VariantBasedNode{std::move(data)} { }

  ExprData(BinaryOperator data) : VariantBasedNode{std::move(data)} { }

  ExprData(UnaryOperator data) : VariantBasedNode{std::move(data)} { }

  ExprData(score::VariantBasedNode<Relation, Pulse, BinaryOperator, UnaryOperator> data)
      : VariantBasedNode{std::move(data)}
  {
  }

  friend bool operator==(const ExprData& lhs, const ExprData& rhs)
  {
    return lhs.m_data == rhs.m_data;
  }

  QString toString() const;
};
}

/**
 * @brief The TreeNode<State::ExprData> class
 *
 * This class is specialized from TreeNode<T>
 * because we want to have an additional check :
 * a node is a leaf iff a node is a State::Relation
 *
 * TODO enforce the invariant of children.size <= 2 (since it's a binary tree)
 */
template <>
class SCORE_LIB_STATE_EXPORT TreeNode<State::ExprData> final : public State::ExprData
{
  //  friend struct TSerializer<DataStream, TreeNode<State::ExprData>>;
  //  friend struct TSerializer<JSONObject, void, TreeNode<State::ExprData>>;

  SCORE_LIB_STATE_EXPORT
  friend bool
  operator!=(const TreeNode<State::ExprData>& lhs, const TreeNode<State::ExprData>& rhs);

  SCORE_LIB_STATE_EXPORT
  friend bool
  operator==(const TreeNode<State::ExprData>& lhs, const TreeNode<State::ExprData>& rhs);

public:
  QString toString() const;
  QString toPrettyString() const;

  using iterator = typename std::list<TreeNode>::iterator;
  using const_iterator = typename std::list<TreeNode>::const_iterator;

  iterator begin();
  const_iterator begin() const;
  const_iterator cbegin() const;
  TreeNode<State::ExprData>& front();

  iterator end();
  const_iterator end() const;
  const_iterator cend() const;
  TreeNode<State::ExprData>& back();

  TreeNode();

  // The parent has to be set afterwards.
  TreeNode(const TreeNode<State::ExprData>& other);
  TreeNode(TreeNode<State::ExprData>&& other);
  TreeNode<State::ExprData>& operator=(const TreeNode<State::ExprData>& source);
  TreeNode<State::ExprData>& operator=(TreeNode<State::ExprData>&& source);

  TreeNode(State::ExprData data, TreeNode* parent) : State::ExprData(std::move(data))
  {
    setParent(parent);
  }

  // Clone
  explicit TreeNode(TreeNode source, TreeNode* parent);
  void push_back(const TreeNode& child);
  void push_back(TreeNode&& child);

  // OPTIMIZEME : the last arg will be this. Is it possible to optimize that ?
  template <typename... Args>
  auto& emplace_back(Args&&... args)
  {
    m_children.emplace_back(std::forward<Args>(args)...);

    auto& cld = m_children.back();
    cld.setParent(this);
    return cld;
  }

  template <typename... Args>
  auto& emplace(Args&&... args)
  {
    auto& n = *m_children.emplace(std::forward<Args>(args)...);
    n.setParent(this);
    return n;
  }

  TreeNode* parent() const;
  bool hasChild(std::size_t index) const;
  TreeNode& childAt(int index);
  const TreeNode& childAt(int index) const;

  // returns -1 if not found
  int indexOfChild(const TreeNode* child) const;
  int childCount() const;
  bool hasChildren() const;

  std::list<TreeNode>& children();
  const std::list<TreeNode>& children() const;

  // Won't delete the child!
  void removeChild(const_iterator it);
  void setParent(TreeNode* parent);

protected:
  TreeNode<State::ExprData>* m_parent{};
  std::list<TreeNode> m_children;
};
bool operator<(const State::ExprData& lhs, const State::ExprData& rhs);

namespace State
{
using Expression = TreeNode<ExprData>;

SCORE_LIB_STATE_EXPORT std::optional<State::Expression> parseExpression(const QString& str);
SCORE_LIB_STATE_EXPORT std::optional<State::Expression> parseExpression(const std::string& str);
SCORE_LIB_STATE_EXPORT State::Expression defaultTrueExpression();
SCORE_LIB_STATE_EXPORT State::Expression defaultFalseExpression();

//! True if the expression is "true" (the default case)
SCORE_LIB_STATE_EXPORT bool isTrueExpression(const QString&);
SCORE_LIB_STATE_EXPORT bool isEmptyExpression(const QString&);
}

JSON_METADATA(State::Address, "Address")
JSON_METADATA(State::AddressAccessor, "AddressAccessor")
JSON_METADATA(State::Relation, "Relation")
JSON_METADATA(State::Pulse, "Pulse")
JSON_METADATA(State::UnaryOperator, "UnOp")
JSON_METADATA(State::BinaryOperator, "BinOp")

Q_DECLARE_METATYPE(State::Expression)
W_REGISTER_ARGTYPE(State::Expression)
