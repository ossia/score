#pragma once
#include <QObject>
#include "Selection.hpp"

class SelectionStack;
class SelectionDispatcher : public QObject
{
    public:
        // Parent must be in the document.
        SelectionDispatcher(QObject* parent);

        SelectionDispatcher(SelectionStack& s, QObject* parent):
            QObject{parent},
            m_stack{s}
        {
        }

        void send(const Selection&);

    private:
        SelectionStack& m_stack;
};
