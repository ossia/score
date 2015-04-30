#pragma once
#include <iscore/selection/Selection.hpp>
#include <QStack>
#include <QDebug>

namespace iscore
{
    class SelectionStack : public QObject
    {
            Q_OBJECT
        public:
            SelectionStack();

            bool canUnselect() const;
            bool canReselect() const;


            // Go to the previous set of selections
            void unselect();

            // Go to the next set of selections
            void reselect();

            // Push a new set of empty selection.
            void deselect();

            Selection currentSelection() const;

        signals:
            void pushNewSelection(const Selection& s);
            void currentSelectionChanged(const Selection&);

        private slots:
            void prune(QObject* p);

        private:
            // Select new objects
            void push(const Selection& s);

            // m_unselectable always contains the empty set at the beginning
            QStack<Selection> m_unselectable;
            QStack<Selection> m_reselectable;
    };
}
