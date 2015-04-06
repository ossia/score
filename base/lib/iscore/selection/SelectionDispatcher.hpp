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

            // Set the current selection, replacing the existing content
            void set(const Selection&);

            // Add item to the current selection
            void append(const Selection::value_type& obj);

            // Remove item from the current selection
            void remove(const Selection::value_type& obj);

            // Useful for oneshots
            void setAndCommit(const Selection&);

            // Sends the whole selection
            void commit();

            iscore::SelectionStack& stack() const;

    private:
            iscore::SelectionStack& m_stack;
            Selection m_selection;
    };
}
