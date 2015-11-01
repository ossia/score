#pragma once
#include <State/Relation.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/VariantBasedNode.hpp>
#include <boost/optional.hpp>
#include <boost/iterator/indirect_iterator.hpp>
namespace iscore
{
enum class BinaryOperator {
    And, Or, Xor
};
enum class UnaryOperator {
    Not
};

struct ExprData : public VariantBasedNode<Relation, BinaryOperator, UnaryOperator>
{
        ISCORE_SERIALIZE_FRIENDS(ExprData, DataStream)
        ISCORE_SERIALIZE_FRIENDS(ExprData, JSONObject)

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

        QString toString() const;
};

}
using iscore::ExprData;

template<>
/**
 * @brief The TreeNode<ExprData> class
 *
 * This class is specialized from TreeNode<T>
 * because we want to have an additional check :
 * a node is a leaf iff a node is a iscore::Relation
 *
 * TODO enforce the invariant of children.size <= 2 (since it's a binary tree)
 */
class TreeNode<ExprData> final : public ExprData
{
        ISCORE_SERIALIZE_FRIENDS(TreeNode<ExprData>, DataStream)
        ISCORE_SERIALIZE_FRIENDS(TreeNode<ExprData>, JSONObject)

        friend bool operator!=(const TreeNode<ExprData>& lhs, const TreeNode<ExprData>& rhs)
        {
            return !(lhs == rhs);
        }

        friend bool operator==(const TreeNode<ExprData>& lhs, const TreeNode<ExprData>& rhs)
        {
            const auto& ltd = static_cast<const ExprData&>(lhs);
            const auto& rtd = static_cast<const ExprData&>(rhs);

            bool b = (ltd == rtd) && (lhs.m_children.size() == rhs.m_children.size());
            if(!b)
                return false;

            for(std::size_t i = 0; i < lhs.m_children.size(); i++)
            {
                if(lhs.m_children[i] != rhs.m_children[i])
                    return false;
            }

            return true;
        }

    public:
        QString toString() const;

        using iterator = typename std::vector<TreeNode>::iterator;
        using const_iterator = typename std::vector<TreeNode>::const_iterator;

        auto begin() { return m_children.begin(); }
        auto begin() const { return m_children.cbegin(); }
        auto cbegin() const { return m_children.cbegin(); }

        auto end() { return m_children.end(); }
        auto end() const { return m_children.cend(); }
        auto cend() const { return m_children.cend(); }

        TreeNode() = default;

        // The parent has to be set afterwards.
        TreeNode(const TreeNode& other):
            ExprData{static_cast<const ExprData&>(other)},
            m_children(other.m_children)
        {
            setParent(other.m_parent);
            for(auto& child : m_children)
                child.setParent(this);
        }

        TreeNode(TreeNode&& other):
            ExprData{static_cast<ExprData&&>(other)},
            m_children(std::move(other.m_children))
        {
            setParent(other.m_parent);
            for(auto& child : m_children)
                child.setParent(this);
        }

        TreeNode& operator=(const TreeNode& source)
        {
            static_cast<ExprData&>(*this) = static_cast<const ExprData&>(source);
            setParent(source.m_parent);

            m_children = source.m_children;
            for(auto& child : m_children)
            {
                child.setParent(this);
            }

            return *this;
        }

        TreeNode& operator=(TreeNode&& source)
        {
            static_cast<ExprData&>(*this) = static_cast<ExprData&&>(source);
            setParent(source.m_parent);

            m_children = std::move(source.m_children);
            for(auto& child : m_children)
            {
                child.setParent(this);
            }

            return *this;
        }

        TreeNode(const ExprData& data, TreeNode* parent):
            ExprData(data)
        {
            setParent(parent);
        }

        // Clone
        explicit TreeNode(
                const TreeNode& source,
                TreeNode* parent):
            TreeNode{source}
        {
            setParent(parent);
        }

        void push_back(const TreeNode& child)
        {
            m_children.push_back(child);

            auto& cld = m_children.back();
            cld.setParent(this);
        }

        void push_back(TreeNode&& child)
        {
            m_children.push_back(std::move(child));

            auto& cld = m_children.back();
            cld.setParent(this);
        }

        template<typename... Args>
        auto& emplace_back(Args&&... args)
        {
            m_children.emplace_back(std::forward<Args>(args)...);

            auto& cld = m_children.back();
            cld.setParent(this);
            return cld;
        }

        template<typename... Args>
        auto& emplace(Args&&... args)
        {
            auto& n = *m_children.emplace(std::forward<Args>(args)...);
            n.setParent(this);
            return n;
        }

        TreeNode* parent() const
        {
            return m_parent;
        }

        bool hasChild(std::size_t index) const
        {
            return m_children.size() > index;
        }

        TreeNode& childAt(int index)
        {
            ISCORE_ASSERT(hasChild(index));
            return m_children.at(index);
        }

        const TreeNode& childAt(int index) const
        {
            ISCORE_ASSERT(hasChild(index));
            return m_children.at(index);
        }

        // returns -1 if not found
        int indexOfChild(const TreeNode* child) const
        {
            for(std::size_t i = 0U; i < m_children.size(); i++)
                if(child == &m_children[i])
                    return i;

            return -1;
        }

        int childCount() const
        { return m_children.size(); }

        bool hasChildren() const
        { return ! m_children.empty(); }

        auto& children()
        { return m_children;  }
        const auto& children() const
        { return m_children;  }

        // Won't delete the child!
        void removeChild(const_iterator it)
        {
            m_children.erase(it);
        }

        void setParent(TreeNode* parent)
        {
            ISCORE_ASSERT(!m_parent || (m_parent && !m_parent->is<iscore::Relation>()));
            m_parent = parent;
        }

    protected:
        TreeNode<ExprData>* m_parent {};
        std::vector<TreeNode> m_children;
};

namespace iscore
{
using Expression = TreeNode<ExprData>;
using Condition = Expression;
using Trigger = Expression;

boost::optional<iscore::Expression> parse(const QString& str);
}

