#pragma once
#include <QObject>
#include <QStack>
#include <QDebug>
using Selection = QObjectList;
class SelectionStack : public QObject
{
        Q_OBJECT
    public:
        SelectionStack()
        {
            m_unselectable.push({});
        }

        bool canUnselect() const
        {
            return m_unselectable.size() > 1;
        }

        bool canReselect() const
        {
            return !m_reselectable.empty();
        }

        // Select new objects
        void push(const Selection& s)
        {
            if(s.toSet() != m_unselectable.top().toSet())
            {
                for(QObject* obj : s)
                {
                    connect(obj, SIGNAL(destroyed(QObject*)),
                            this, SLOT(prune(QObject*)));
                }
                m_unselectable.push(s);
                m_reselectable.clear();

                emit currentSelectionChanged(s);
            }
        }

        // Go to the previous set of selections
        void unselect()
        {
            m_reselectable.push(m_unselectable.pop());

            emit currentSelectionChanged(m_unselectable.top());
        }

        // Go to the next set of selections
        void reselect()
        {
            m_unselectable.push(m_reselectable.pop());

            emit currentSelectionChanged(m_unselectable.top());
        }

        // Push a new set of empty selection.
        void deselect()
        {
            push({});
        }

    signals:
        void currentSelectionChanged(Selection);

    private slots:
        void prune(QObject* p)
        {
            for(auto& sel : m_unselectable)
            {
                sel.removeAll(p);
            }

            for(auto& sel : m_reselectable)
            {
                sel.removeAll(p);
            }

            emit currentSelectionChanged(m_unselectable.top());

        }

    private:
        // m_unselectable always contains the empty set at the beginning
        QStack<Selection> m_unselectable;
        QStack<Selection> m_reselectable;
};
