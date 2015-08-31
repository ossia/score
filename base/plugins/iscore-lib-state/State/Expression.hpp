#pragma once
#include <QString>
#include <State/Address.hpp>
#include <State/Value.hpp>
#include <eggs/variant.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/VariantBasedNode.hpp>
namespace iscore
{
using RelationMember = eggs::variant<iscore::Address, iscore::Value>;

struct Relation
{
        enum Operator {
            Equal,
            Different,
            Greater,
            Lower,
            GreaterEqual,
            LowerEqual
        } op;

        RelationMember lhs, rhs;

        friend bool operator==(const Relation& lhs, const Relation& rhs)
        {
            return lhs.lhs == rhs.lhs && lhs.rhs == rhs.rhs && lhs.op == rhs.op;
        }
};

enum class BoolOperator {
    And, Or, Xor
};

struct Expression : public VariantBasedNode<Relation, BoolOperator>
{
        friend bool operator==(const Expression& lhs, const Expression& rhs)
        {
            return lhs.m_data == rhs.m_data;
        }

};

using Condition = Expression;
using Trigger = Expression;
}
using iscore::Expression;
template<>
class TreeNode<Expression> : public Expression
{
        ISCORE_SERIALIZE_FRIENDS(TreeNode<Expression>, DataStream)
        ISCORE_SERIALIZE_FRIENDS(TreeNode<Expression>, JSONObject)

    public:
        TreeNode():
            Expression{}
        {

        }

        TreeNode(const Expression& data):
            Expression{data}
        {

        }

        TreeNode(Expression&& data):
            Expression{std::move(data)}
        {

        }

        template<typename T>
        TreeNode(const T& data, TreeNode<Expression> * parent):
            Expression(data),
            m_parent{parent}
        {
            if(m_parent) {
                m_parent->addChild(this);
            }
        }

        // Clone
        TreeNode(const TreeNode<Expression>& source,
                 TreeNode<Expression>* parent = nullptr):
            Expression{static_cast<const Expression&>(source)},
            m_parent{parent}
        {
            for(const auto& child : source.children())
            {
                this->addChild(new TreeNode<Expression>{*child, this});
            }
        }

        TreeNode& operator=(const TreeNode<Expression>& source)
        {
            static_cast<Expression&>(*this) = static_cast<const Expression&>(source);

            qDeleteAll(m_children);
            for(const auto& child : source.children())
            {
                this->addChild(new TreeNode<Expression>{*child, this});
            }

            return *this;
        }



        ~TreeNode()
        {
            qDeleteAll(m_children);
        }

        void setParent(TreeNode<Expression>* parent)
        {
            ISCORE_ASSERT(!parent->is<iscore::Relation>());
            if(m_parent)
                m_parent->removeChild(this);

            m_parent = parent;
            m_parent->addChild(this);
        }

        TreeNode<Expression>* parent() const
        {
            return m_parent;
        }

        // returns 0 if invalid index
        TreeNode<Expression>* childAt(int index) const
        { return m_children.value(index); }

        // returns -1 if not found
        int indexOfChild(const TreeNode<Expression>* child) const
        { return m_children.indexOf(const_cast<TreeNode<Expression>*>(child)); }

        int childCount() const
        { return m_children.count(); }

        bool hasChildren() const
        { return ! m_children.empty(); }

        QList<TreeNode<Expression>*> children() const
        { return m_children;  }

        void insertChild(int index, TreeNode<Expression>* n)
        {
            ISCORE_ASSERT(n);
            n->m_parent = this;
            m_children.insert(index, n);
        }

        void addChild(TreeNode<Expression>* n)
        {
            ISCORE_ASSERT(n);

            ISCORE_ASSERT(!this->is<iscore::Relation>());
            n->m_parent = this;
            m_children.append(n);
        }

        void swapChildren(int oldIndex, int newIndex)
        {
            ISCORE_ASSERT(oldIndex < m_children.count());
            ISCORE_ASSERT(newIndex < m_children.count());

            m_children.swap(oldIndex, newIndex);
        }

        TreeNode<Expression>* takeChild(int index)
        {
            TreeNode* n = m_children.takeAt(index);
            ISCORE_ASSERT(n);
            n->m_parent = 0;
            return n;
        }

        // Won't delete the child!
        void removeChild(TreeNode<Expression>* child)
        {
            m_children.removeAll(child);
        }


    protected:
        TreeNode<Expression>* m_parent {};
        QList<TreeNode<Expression>*> m_children;

};

using ExprTree = TreeNode<Expression>;
