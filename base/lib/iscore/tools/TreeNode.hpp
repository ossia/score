#pragma once
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
/*
 * @brief The TreeNode class
 *
 * This class adds a tree structure around a data type.
 * It can then be used in abstract item models easily.
 */
template<typename DataType>
class TreeNode : public DataType
{
    public:
        using iterator = typename std::vector<TreeNode>::iterator;
        using const_iterator = typename std::vector<TreeNode>::const_iterator;

        auto begin() { return m_children.begin(); }
        auto begin() const { return cbegin(); }
        auto cbegin() const { return m_children.cbegin(); }

        auto end() { return m_children.end(); }
        auto end() const { return cend(); }
        auto cend() const { return m_children.cend(); }

        TreeNode() = default;

        // The parent has to be set afterwards.
        TreeNode(const TreeNode& other):
            DataType{static_cast<const DataType&>(other)},
            m_parent{other.m_parent},
            m_children(other.m_children)
        {
            for(auto& child : m_children)
                child.setParent(this);
        }

        TreeNode(TreeNode&& other):
            DataType{static_cast<DataType&&>(other)},
            m_parent{other.m_parent},
            m_children(std::move(other.m_children))
        {
            for(auto& child : m_children)
                child.setParent(this);
        }

        TreeNode& operator=(const TreeNode& source)
        {
            static_cast<DataType&>(*this) = static_cast<const DataType&>(source);
            m_parent = source.m_parent;

            m_children = source.m_children;
            for(auto& child : m_children)
            {
                child.setParent(this);
            }

            return *this;
        }

        TreeNode& operator=(TreeNode&& source)
        {
            static_cast<DataType&>(*this) = static_cast<DataType&&>(source);
            m_parent = source.m_parent;

            m_children = std::move(source.m_children);
            for(auto& child : m_children)
            {
                child.setParent(this);
            }

            return *this;
        }

        TreeNode(const DataType& data, TreeNode* parent):
            DataType(data),
            m_parent{parent}
        {
        }

        // Clone
        explicit TreeNode(
                 const TreeNode& source,
                 TreeNode* parent):
            TreeNode{source}
        {
            m_parent = parent;
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

        auto iterOfChild(const TreeNode* child)
        {
            auto end = m_children.end();
            for(auto it = m_children.begin(); it != end; ++it)
            {
                if(&*it == child)
                    return it;
            }
            return end;
        }

        int childCount() const
        { return m_children.size(); }

        bool hasChildren() const
        { return ! m_children.empty(); }

        const auto& children() const
        { return m_children;  }
        auto& children()
        { return m_children;  }

        void swapChildren(int oldIndex, int newIndex)
        {
            ISCORE_ASSERT(oldIndex < m_children.size());
            ISCORE_ASSERT(newIndex < m_children.size());

            m_children.swap(oldIndex, newIndex);
        }

        // Won't delete the child!
        void removeChild(const_iterator it)
        {
            m_children.erase(it);
        }

        void setParent(TreeNode* parent)
        {
            m_parent = parent;
        }

    private:
        TreeNode* m_parent {};
        std::vector<TreeNode> m_children;
};


template<typename T>
void Visitor<Reader<DataStream>>::readFrom(const TreeNode<T>& n)
{
    readFrom(static_cast<const T&>(n));

    m_stream << n.childCount();
    for(const auto& child : n)
    {
        readFrom(child);
    }

    insertDelimiter();
}


template<typename T>
void Visitor<Writer<DataStream>>::writeTo(TreeNode<T>& n)
{
    writeTo(static_cast<T&>(n));

    int childCount;
    m_stream >> childCount;
    for (int i = 0; i < childCount; ++i)
    {
        TreeNode<T> child;
        writeTo(child);
        n.push_back(std::move(child));
    }

    checkDelimiter();
}

template<typename T>
void Visitor<Reader<JSONObject>>::readFrom(const TreeNode<T>& n)
{
    readFrom(static_cast<const T&>(n));
    m_obj["Children"] = toJsonArray(n);
}

template<typename T>
void Visitor<Writer<JSONObject>>::writeTo(TreeNode<T>& n)
{
    writeTo(static_cast<T&>(n));
    for (const auto& val : m_obj["Children"].toArray())
    {
        TreeNode<T> child;
        Deserializer<JSONObject> nodeWriter(val.toObject());

        nodeWriter.writeTo(child);
        n.push_back(std::move(child));
    }
}
