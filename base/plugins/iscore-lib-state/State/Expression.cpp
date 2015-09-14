#include "Expression.hpp"
#include <algorithm>


QString ExprData::toString() const
{
    static const QMap<iscore::BinaryOperator, QString> binopMap{
      { iscore::BinaryOperator::And, "and" },
      { iscore::BinaryOperator::Or, "or" },
      { iscore::BinaryOperator::Xor, "xor" },
    };
    static const QMap<iscore::UnaryOperator, QString> unopMap{
        { iscore::UnaryOperator::Not, "not" },
    };

    if(is<iscore::Relation>())
        return get<iscore::Relation>().toString();
    else if(is<iscore::BinaryOperator>())
         return binopMap[(get<iscore::BinaryOperator>())];
    else if(is<iscore::UnaryOperator>())
        return unopMap[(get<iscore::UnaryOperator>())];
    else if(is<InvisibleRootNodeTag>())
        return "";
    else
        ISCORE_ABORT;
}

QString TreeNode<ExprData>::toString() const
{
    QString s;

    auto exprstr = static_cast<const iscore::ExprData&>(*this).toString();
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

