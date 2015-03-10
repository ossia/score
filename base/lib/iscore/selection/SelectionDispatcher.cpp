#include "SelectionDispatcher.hpp"
#include "SelectionStack.hpp"

using namespace iscore;

void SelectionDispatcher::send(const Selection& s)
{
    m_stack.push(s);
}
