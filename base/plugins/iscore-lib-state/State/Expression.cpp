// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QMap>
#include <QStringBuilder>
#include "Expression.hpp"
#include <State/Relation.hpp>

QString State::ExprData::toString() const
{
  static const QMap<State::BinaryOperator, QString> binopMap{
      {State::BinaryOperator::AND, "and"},
      {State::BinaryOperator::OR, "or"},
      {State::BinaryOperator::XOR, "xor"},
  };
  static const QMap<State::UnaryOperator, QString> unopMap{
      {State::UnaryOperator::Not, "not"},
  };

  struct vis
  {
  public:
    using return_type = QString;
    return_type operator()(const State::Relation& rel) const
    {
      return State::toString(rel);
    }
    return_type operator()(const State::Pulse& rel) const
    {
      return State::toString(rel);
    }

    return_type operator()(const State::BinaryOperator rel) const
    {
      return binopMap[rel];
    }
    return_type operator()(const State::UnaryOperator rel) const
    {
      return unopMap[rel];
    }
    return_type operator()(const InvisibleRootNode rel) const
    {
      return "";
    }
  };

  return eggs::variants::apply(vis{}, m_data);
}

QString TreeNode<State::ExprData>::toString() const
{
  QString s;

  auto exprstr = static_cast<const State::ExprData&>(*this).toString();
  if (m_children.empty()) // Relation
  {
    if (this->is<InvisibleRootNode>())
    {
      ;
    }
    else
    {
      s = " { " % exprstr % " } ";
    }
  }
  else if (m_children.size() == 1) // unop
  {
    if (this->is<InvisibleRootNode>())
    {
      s = m_children.at(0).toString();
    }
    else
    {
      s = " { " % exprstr % " " % m_children.at(0).toString() % " } ";
    }
  }
  else // binop
  {
    ISCORE_ASSERT(m_children.size() == 2);
    int n = 0;
    int max_n = m_children.size() - 1;
    for (const auto& child : m_children)
    {
      s += child.toString() % " ";
      if (n < max_n)
      {
        s += exprstr % " ";
        n++;
      }
    }
  }

  return s;
}

State::Expression State::defaultTrueExpression()
{
  using namespace std::literals;
  static const auto expr = *State::parseExpression("true == true"s);
  return expr;
}

State::Expression State::defaultFalseExpression()
{
  using namespace std::literals;
  static const auto expr = *State::parseExpression("true == false"s);
  return expr;
}

bool State::isTrueExpression(const QString& cond)
{
  return cond.isEmpty() || cond == " { true == true } " || cond == "true == true";
}

bool State::isEmptyExpression(const QString& cond)
{
  return cond.isEmpty();
}
