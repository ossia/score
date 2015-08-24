#pragma once
#include <QList>
#include <QModelIndex>

template<typename T>
using ref = T&;
template<typename T>
using cref = const T&;

// Sadly we can't have a non-const interface
// because of QList<Node*> in Node::children...

template<typename T>
class TreePath
{
    public:
        TreePath() = default;
        TreePath(const QList<int>& other):
            m_path(other)
        {

        }

        TreePath(QModelIndex index)
        {
            QModelIndex iter = index;

            while(iter.isValid())
            {
                m_path.prepend(iter.row());
                iter = iter.parent();
            }
        }

        TreePath(const T& node)
        {
            // We have to take care of the root node.
            if(node.template is<InvisibleRootNodeTag>())
                return;

            auto iter = &node;
            while(iter && !iter->template is<InvisibleRootNodeTag>())
            {
                m_path.prepend(iter->parent()->indexOfChild(iter));
                iter = iter->parent();
            }
        }

        T* toNode(T* iter) const
        {
            const int pathSize = m_path.size();

            for (int i = 0; i < pathSize; ++i)
            {
                if (m_path.at(i) < iter->childCount())
                {
                    iter = iter->childAt(m_path.at(i));
                }
                else
                {
                    return nullptr;
                }
            }

            return iter;
        }

        void append(int i)
        { m_path.append(i); }

        void prepend(int i)
        { m_path.prepend(i); }

        const int& at(int i) const
        { return m_path.at(i); }

        int &back()
        { return m_path.back(); }

        int size() const
        { return m_path.size(); }
        bool empty() const
        { return m_path.empty(); }

        void removeLast()
        { m_path.removeLast(); }

        void clear()
        { m_path.clear(); }

        void reserve(int size)
        { m_path.reserve(size); }

        const QList<int>& toList() const
        { return m_path; }

        operator ref<QList<int>>()
        { return m_path; }

        operator cref<QList<int>>() const
        { return m_path; }

    private:
        QList<int> m_path;
};

#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
template<typename T>
void Visitor<Reader<DataStream>>::readFrom(const TreePath<T>& path)
{
    m_stream << static_cast<const QList<int>&>(path);
}

template<typename T>
void Visitor<Writer<DataStream>>::writeTo(TreePath<T>& path)
{
    m_stream >> static_cast<QList<int>&>(path);
}

template<typename T>
void Visitor<Reader<JSONObject>>::readFrom(const TreePath<T>& path)
{
    m_obj["Path"] = toJsonArray(static_cast<const QList<int>&>(path));
}

template<typename T>
void Visitor<Writer<JSONObject>>::writeTo(TreePath<T>& path)
{
    fromJsonArray(m_obj["Path"].toArray(), static_cast<QList<int>&>(path));
}
