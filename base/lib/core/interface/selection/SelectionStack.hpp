#pragma once
#include <core/interface/selection/Selection.hpp>
#include <QStack>
#include <QDebug>

class SelectionStack : public QObject
{
        Q_OBJECT
    public:
        SelectionStack();

        bool canUnselect() const;
        bool canReselect() const;

        // Select new objects
        void push(const Selection& s);

        // Go to the previous set of selections
        void unselect();

        // Go to the next set of selections
        void reselect();

        // Push a new set of empty selection.
        void deselect();

    signals:
        void currentSelectionChanged(const Selection&);

    private slots:
        void prune(QObject* p);

    private:
        // m_unselectable always contains the empty set at the beginning
        QStack<Selection> m_unselectable;
        QStack<Selection> m_reselectable;
};
