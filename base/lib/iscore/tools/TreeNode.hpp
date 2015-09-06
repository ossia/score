#pragma once
#include <QList>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <boost/iterator/indirect_iterator.hpp>
/**
 * @brief The TreeNode class
 *
 * This class adds a tree structure around a data type.
 * It can then be used in abstract item models easily.
 */
template<typename DataType>
class TreeNode : public DataType
{
        friend void Visitor<Reader< DataStream >>::readFrom< TreeNode > (const TreeNode &);/*
        friend void Visitor<Writer< DataStream >>::writeTo< TreeNode<U> > (TreeNode<U> &);
        friend void Visitor<Reader< JSONObject >>::readFrom< TreeNode<U> > (const TreeNode<U> &);
        friend void Visitor<Writer< JSONObject >>::writeTo< TreeNode<U> > (TreeNode<U> &);*/

    public:
        TreeNode():
            DataType{}
        {

        }

        TreeNode(const DataType& data):
            DataType{data}
        {

        }

        TreeNode(DataType&& data):
            DataType{std::move(data)}
        {

        }

        template<typename T>
        TreeNode(const T& data, TreeNode* parent):
            DataType(data),
            m_parent{parent}
        {
            if(m_parent) {
                m_parent->addChild(this);
            }
        }

        // Clone
        TreeNode(const TreeNode& source,
                 TreeNode* parent = nullptr):
            DataType{static_cast<const DataType&>(source)},
            m_parent{parent}
        {
            for(const auto& child : source.m_children)
            {
                this->addChild(new TreeNode{*child, this});
            }
        }

        TreeNode& operator=(const TreeNode& source)
        {
            static_cast<DataType&>(*this) = static_cast<const DataType&>(source);

            qDeleteAll(m_children);
            m_children.clear();
            for(const auto& child : source.m_children)
            {
                this->addChild(new TreeNode{*child, this});
            }

            return *this;
        }



        ~TreeNode()
        {
            qDeleteAll(m_children);
        }

        void setParent(TreeNode* parent)
        {
            if(m_parent)
                m_parent->removeChild(this);

            m_parent = parent;
            m_parent->addChild(this);
        }

        TreeNode* parent() const
        {
            return m_parent;
        }

        // returns 0 if invalid index
        TreeNode* childAt(int index) const
        { return m_children.value(index); }

        // returns -1 if not found
        int indexOfChild(const TreeNode* child) const
        { return m_children.indexOf(const_cast<TreeNode*>(child)); }

        int childCount() const
        { return m_children.count(); }

        bool hasChildren() const
        { return ! m_children.empty(); }

        QList<TreeNode*> children() const
        { return m_children;  }

        void insertChild(int index, TreeNode* n)
        {
            ISCORE_ASSERT(n);
            n->m_parent = this;
            m_children.insert(index, n);
        }

        void addChild(TreeNode* n)
        {
            ISCORE_ASSERT(n);
            n->m_parent = this;
            m_children.append(n);
        }

        void swapChildren(int oldIndex, int newIndex)
        {
            ISCORE_ASSERT(oldIndex < m_children.count());
            ISCORE_ASSERT(newIndex < m_children.count());

            m_children.swap(oldIndex, newIndex);
        }

        TreeNode* takeChild(int index)
        {
            TreeNode* n = m_children.takeAt(index);
            ISCORE_ASSERT(n);
            n->m_parent = 0;
            return n;
        }

        // Won't delete the child!
        void removeChild(TreeNode* child)
        {
            m_children.removeAll(child);
        }

        auto begin() const { return boost::make_indirect_iterator(m_children.begin()); }
        auto cbegin() const { return boost::make_indirect_iterator(m_children.cbegin()); }
        auto end() const { return boost::make_indirect_iterator(m_children.end()); }
        auto cend() const { return boost::make_indirect_iterator(m_children.cend()); }

    private:
        TreeNode* m_parent {};
        QList<TreeNode*> m_children;
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
        auto child = new TreeNode<T>;
        writeTo(*child);
        n.addChild(child);
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
        auto child = new TreeNode<T>;
        Deserializer<JSONObject> nodeWriter(val.toObject());

        nodeWriter.writeTo(*child);
        n.addChild(child);
    }
}
