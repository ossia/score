#pragma once
#include <QObject>
#include <QStack>
#include <QDebug>
using Selection = QObjectList;
class SelectionStack : public QObject
{
        Q_OBJECT
    public:
        using QObject::QObject;

        bool canUnselect() const
        {
            return !m_unselectable.empty();
        }

        bool canReselect() const
        {
            return !m_reselectable.empty();
        }

        // Select new objects
        void push(const Selection& s)
        {
            if(m_unselectable.empty() ||
               s.toSet() != m_unselectable.top().toSet())
            {
                m_unselectable.push(s);
                m_reselectable.clear();

                emit currentSelectionChanged(s);
            }
        }

        // Go to the previous set of selections
        void unselect()
        {
            m_reselectable.push(m_unselectable.pop());

            emit currentSelectionChanged(m_unselectable.empty() ? Selection{} : m_unselectable.top());
        }

        // Go to the next set of selections
        void reselect()
        {
            m_unselectable.push(m_reselectable.pop());

            emit currentSelectionChanged(m_unselectable.empty() ? Selection{} : m_unselectable.top());
        }

        // Push a new set of empty selection.
        void deselect()
        {
            if(!m_unselectable.top().empty())
            {
                push({});
            }
        }

    signals:
        void currentSelectionChanged(Selection);

    private:
        QStack<Selection> m_unselectable;
        QStack<Selection> m_reselectable;
};
