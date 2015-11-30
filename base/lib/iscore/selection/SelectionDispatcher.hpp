#pragma once
#include "Selection.hpp"

namespace iscore
{
class SelectionStack;

/**
 * @brief The SelectionDispatcher class
 *
 * Sends new selections to the document so that they
 * can be shown in the inspector and other places that work with selections.
 */
class SelectionDispatcher
{
    public:
        explicit SelectionDispatcher(SelectionStack& s):
            m_stack{s}
        {
        }

        void setAndCommit(const Selection&);

        iscore::SelectionStack& stack() const;

    private:
        iscore::SelectionStack& m_stack;
};
}
