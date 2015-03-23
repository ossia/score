#pragma once
#include <QObject>
#include "Selection.hpp"

namespace iscore
{
    class SelectionStack;
    class SelectionDispatcher
    {
        public:
            SelectionDispatcher(SelectionStack& s):
                m_stack{s}
            {
            }

            void send(const Selection&);

            iscore::SelectionStack &stack() const;

    private:
            iscore::SelectionStack& m_stack;
    };
}
