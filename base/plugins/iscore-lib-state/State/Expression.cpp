#include "Expression.hpp"
#include <algorithm>

TreeNode<ExprData>::TreeNode():
    ExprData{}
{

}


TreeNode<ExprData>::TreeNode(const iscore::ExprData& data):
    ExprData{data}
{

}


TreeNode<ExprData>::TreeNode(iscore::ExprData&& data):
    ExprData{std::move(data)}
{

}

TreeNode<ExprData>::TreeNode(const TreeNode<iscore::ExprData>& source, TreeNode<iscore::ExprData>* parent):
    ExprData{static_cast<const ExprData&>(source)},
    m_parent{parent}
{
    for(const auto& child : source.m_children)
    {
        this->addChild(new TreeNode<ExprData>{*child, this});
    }
}


TreeNode<ExprData>&TreeNode<ExprData>::operator=(const TreeNode<iscore::ExprData>& source)
{
    static_cast<ExprData&>(*this) = static_cast<const ExprData&>(source);

    qDeleteAll(m_children);
    m_children.clear();

    for(const auto& child : source.m_children)
    {
        this->addChild(new TreeNode<ExprData>{*child, this});
    }

    return *this;
}


TreeNode<ExprData>::~TreeNode()
{
    qDeleteAll(m_children);
}


void TreeNode<ExprData>::setParent(TreeNode<iscore::ExprData>* parent)
{
    ISCORE_ASSERT(!parent->is<iscore::Relation>());
    if(m_parent)
        m_parent->removeChild(this);

    m_parent = parent;
    m_parent->addChild(this);
}


TreeNode<iscore::ExprData>*TreeNode<ExprData>::parent() const
{
    return m_parent;
}


void TreeNode<ExprData>::insertChild(int index, TreeNode<iscore::ExprData>* n)
{
    ISCORE_ASSERT(n);
    n->m_parent = this;
    m_children.insert(index, n);
}


void TreeNode<ExprData>::addChild(TreeNode<iscore::ExprData>* n)
{
    ISCORE_ASSERT(n);

    ISCORE_ASSERT(!this->is<iscore::Relation>());
    n->m_parent = this;
    m_children.append(n);
}


void TreeNode<ExprData>::swapChildren(int oldIndex, int newIndex)
{
    ISCORE_ASSERT(oldIndex < m_children.count());
    ISCORE_ASSERT(newIndex < m_children.count());

    m_children.swap(oldIndex, newIndex);
}


TreeNode<iscore::ExprData>*TreeNode<ExprData>::takeChild(int index)
{
    TreeNode* n = m_children.takeAt(index);
    ISCORE_ASSERT(n);
    n->m_parent = 0;
    return n;
}


void TreeNode<ExprData>::removeChild(TreeNode<iscore::ExprData>* child)
{
    m_children.removeAll(child);
}




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
    if(m_children.count() == 0) // Relation
    {
        s = "(" + exprstr + ")";
    }
    else if(m_children.count() == 1) // unop
    {
        s = "(" + exprstr + " " + m_children.at(0)->toString() + ")";
    }
    else // binop
    {
        ISCORE_ASSERT(m_children.count() == 2);
        int n = 0;
        int max_n = m_children.count() - 1;
        for(const auto& child : m_children)
        {
            s += child->toString() + " ";
            if(n < max_n)
            {
                s += exprstr + " ";
                n++;
            }
        }
    }

    return s;
}

