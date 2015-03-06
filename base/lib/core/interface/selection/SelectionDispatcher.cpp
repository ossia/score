#include "SelectionDispatcher.hpp"
#include "SelectionStack.hpp"

#include <core/interface/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentPresenter.hpp>

using namespace iscore;
SelectionDispatcher::SelectionDispatcher(QObject* parent):
    SelectionDispatcher{IDocument::documentFromObject(parent)->selectionStack(),
                        parent}
{

}

void SelectionDispatcher::send(const Selection& s)
{
    m_stack.push(s);
}
