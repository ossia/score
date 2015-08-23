#pragma once
#include <QList>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

struct InvisibleRootNodeTag{};

template<typename DataType>
class TreeNode : public DataType
{
        ISCORE_SERIALIZE_FRIENDS(TreeNode, DataStream)
        ISCORE_SERIALIZE_FRIENDS(TreeNode, JSONObject)

    public:
        TreeNode():
            DataType{}
        {

        }

        TreeNode(const DataType& data):
            DataType{data}
        {

        }

        TreeNode(const DataType& data, TreeNode* parent):
            DataType{data},
            m_parent{parent}
        {
            if(m_parent)
            {
                m_parent->addChild(this);
            }
        }

        // Clone
        TreeNode(const TreeNode& source,
             TreeNode* parent = nullptr):
            DataType{static_cast<const DataType&>(source)},
            m_parent{parent}
        {
            for(const auto& child : source.children())
            {
                this->addChild(new TreeNode{*child, this});
            }
        }

        TreeNode& operator=(const TreeNode& source)
        {
            static_cast<DataType&>(*this) = static_cast<const DataType&>(source);

            qDeleteAll(m_children);
            for(const auto& child : source.children())
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
            Q_ASSERT(n);
            n->m_parent = this;
            m_children.insert(index, n);
        }

        void addChild(TreeNode* n)
        {
            Q_ASSERT(n);
            n->m_parent = this;
            m_children.append(n);
        }

        void swapChildren(int oldIndex, int newIndex)
        {
            Q_ASSERT(oldIndex < m_children.count());
            Q_ASSERT(newIndex < m_children.count());

            m_children.swap(oldIndex, newIndex);
        }

        TreeNode* takeChild(int index)
        {
            TreeNode* n = m_children.takeAt(index);
            Q_ASSERT(n);
            n->m_parent = 0;
            return n;
        }

        // Won't delete the child!
        void removeChild(TreeNode* child)
        {
            m_children.removeAll(child);
        }


    protected:
        TreeNode* m_parent {};
        QList<TreeNode*> m_children;
};


        template<typename T>
        void Visitor<Reader<DataStream>>::readFrom(const TreeNode<T>& n)
        {
            readFrom(static_cast<const T&>(n));

            m_stream << n.childCount();
            for(auto& child : n.children())
            {
                if(child) readFrom(*child);
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
            m_obj["Children"] = toJsonArray(n.children());
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
