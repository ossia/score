#pragma once

#include <QList>

class Node;

class QModelIndex;

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

        void serializePath(QDataStream& d) const;
        void deserializePath(QDataStream& d);

        QList<int> toList() const
        { return m_path; }


    private:
        QList<int> m_path;
};
