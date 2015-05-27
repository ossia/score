#pragma once

#include <QList>

class Node;

class QModelIndex;
template<typename T>
using ref = T&;
template<typename T>
using cref = const T&;

// TODO Rename in NodePath to prevent confusion ?
class Path
{
    public:
        Path();
        Path(const QList<int>& other):
            m_path{other}
        {

        }

        Path(QModelIndex index);
        Path(Node* node);

        Node* toNode(Node* iter);

        void append(int i);
        void prepend(int i);

        const int& at(int i) const;
        int &back();

        int size() const;

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
