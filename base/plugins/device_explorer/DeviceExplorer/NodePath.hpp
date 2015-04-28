#pragma once

#include <QList>

class Node;

class QModelIndex;

class Path
{
    public:
        Path();
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

    private:
        QList<int> m_path;
};
