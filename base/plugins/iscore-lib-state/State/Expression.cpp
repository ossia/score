#include <QMap>

#include "Expression.hpp"
#include <State/Relation.hpp>

QString ExprData::toString() const
{
    static const QMap<State::BinaryOperator, QString> binopMap{
      { State::BinaryOperator::And, "and" },
      { State::BinaryOperator::Or, "or" },
      { State::BinaryOperator::Xor, "xor" },
    };
    static const QMap<State::UnaryOperator, QString> unopMap{
        { State::UnaryOperator::Not, "not" },
    };

    if(is<State::Relation>())
        return get<State::Relation>().toString();
    else if(is<State::BinaryOperator>())
         return binopMap[(get<State::BinaryOperator>())];
    else if(is<State::UnaryOperator>())
        return unopMap[(get<State::UnaryOperator>())];
    else if(is<InvisibleRootNodeTag>())
        return "";
    else
        ISCORE_ABORT;
}

QString TreeNode<ExprData>::toString() const
{
    QString s;

    auto exprstr = static_cast<const State::ExprData&>(*this).toString();
    if(m_children.size() == 0) // Relation
    {
        if(is<InvisibleRootNodeTag>())
        {
            ;
        }
        else
        {
            s = "(" + exprstr + ")";
        }
    }
    else if(m_children.size() == 1) // unop
    {
        if(is<InvisibleRootNodeTag>())
        {
            s = m_children.at(0).toString();
        }
        else
        {
            s = "(" + exprstr + " " + m_children.at(0).toString() + ")";
        }
    }
    else // binop
    {
        ISCORE_ASSERT(m_children.size() == 2);
        int n = 0;
        int max_n = m_children.size() - 1;
        for(const auto& child : m_children)
        {
            s += child.toString() + " ";
            if(n < max_n)
            {
                s += exprstr + " ";
                n++;
            }
        }
    }

    return s;
}

