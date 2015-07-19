#pragma once

#include <QList>

namespace iscore
{
class Node;
}
class QModelIndex;
template<typename T>
using ref = T&;
template<typename T>
using cref = const T&;

// Sadly we can't have a non-const interface
// because of QList<Node*> in Node::children...
class NodePath
{
    public:
        NodePath();
        NodePath(const QList<int>& other):
            m_path{other}
        {

        }

        NodePath(QModelIndex index);
        NodePath(iscore::Node& node);

        iscore::Node* toNode(iscore::Node* iter);

        void append(int i);
        void prepend(int i);

        const int& at(int i) const;
        int &back();

        int size() const;
        bool empty() const;

        void removeLast();
        void clear();
        void reserve(int size);

        const QList<int>& toList() const
        { return m_path; }

        operator ref<QList<int>>()
        { return m_path; }

        operator cref<QList<int>>() const
        { return m_path; }

    private:
        QList<int> m_path;
};
