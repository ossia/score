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
        } ;

        RelationMember lhs;
        Operator op;
        RelationMember rhs;

        friend bool operator==(const Relation& lhs, const Relation& rhs)
        {
            return lhs.lhs == rhs.lhs && lhs.rhs == rhs.rhs && lhs.op == rhs.op;
        }
};

enum class BoolOperator {
    And, Or, Xor
};

struct ExprData : public VariantBasedNode<Relation, BoolOperator>
{
        ExprData() = default;
        template<typename T>
        ExprData(const T& data):
            VariantBasedNode{data}
        {

        }

        friend bool operator==(const ExprData& lhs, const ExprData& rhs)
        {
            return lhs.m_data == rhs.m_data;
        }

};

}
using iscore::ExprData;
template<>
class TreeNode<ExprData> : public ExprData
{
        ISCORE_SERIALIZE_FRIENDS(TreeNode<ExprData>, DataStream)
        ISCORE_SERIALIZE_FRIENDS(TreeNode<ExprData>, JSONObject)

    public:
        TreeNode():
            ExprData{}
        {

        }

        TreeNode(const ExprData& data):
            ExprData{data}
        {

        }

        TreeNode(ExprData&& data):
            ExprData{std::move(data)}
        {

        }

        template<typename T>
        TreeNode(const T& data, TreeNode<ExprData> * parent):
            ExprData(data),
            m_parent{parent}
        {
            if(m_parent) {
                m_parent->addChild(this);
            }
        }

        // Clone
        TreeNode(const TreeNode<ExprData>& source,
                 TreeNode<ExprData>* parent = nullptr):
            ExprData{static_cast<const ExprData&>(source)},
            m_parent{parent}
        {
            for(const auto& child : source.children())
            {
                this->addChild(new TreeNode<ExprData>{*child, this});
            }
        }

        TreeNode& operator=(const TreeNode<ExprData>& source)
        {
            static_cast<ExprData&>(*this) = static_cast<const ExprData&>(source);

            qDeleteAll(m_children);
            for(const auto& child : source.children())
            {
                this->addChild(new TreeNode<ExprData>{*child, this});
            }

            return *this;
        }



        ~TreeNode()
        {
            qDeleteAll(m_children);
        }

        void setParent(TreeNode<ExprData>* parent)
        {
            ISCORE_ASSERT(!parent->is<iscore::Relation>());
            if(m_parent)
                m_parent->removeChild(this);

            m_parent = parent;
            m_parent->addChild(this);
        }

        TreeNode<ExprData>* parent() const
        {
            return m_parent;
        }

        // returns 0 if invalid index
        TreeNode<ExprData>* childAt(int index) const
        { return m_children.value(index); }

        // returns -1 if not found
        int indexOfChild(const TreeNode<ExprData>* child) const
        { return m_children.indexOf(const_cast<TreeNode<ExprData>*>(child)); }

        int childCount() const
        { return m_children.count(); }

        bool hasChildren() const
        { return ! m_children.empty(); }

        QList<TreeNode<ExprData>*> children() const
        { return m_children;  }

        void insertChild(int index, TreeNode<ExprData>* n)
        {
            ISCORE_ASSERT(n);
            n->m_parent = this;
            m_children.insert(index, n);
        }

        void addChild(TreeNode<ExprData>* n)
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

        TreeNode<ExprData>* takeChild(int index)
        {
            TreeNode* n = m_children.takeAt(index);
            ISCORE_ASSERT(n);
            n->m_parent = 0;
            return n;
        }

        // Won't delete the child!
        void removeChild(TreeNode<ExprData>* child)
        {
            m_children.removeAll(child);
        }


    protected:
        TreeNode<ExprData>* m_parent {};
        QList<TreeNode<ExprData>*> m_children;

};

namespace iscore
{
using Expression = TreeNode<ExprData>;
using Condition = ExprData;
using Trigger = ExprData;

iscore::Expression parse(const QString& str);
bool validate(const Expression& expr);
}


void expr_parse_test();
