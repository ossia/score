#include "StateTreeView.hpp"

#include <State/Widgets/MessageWidget.hpp>
void StateTreeView::mouseDoubleClickEvent(QMouseEvent* ev)
{
    QTreeView::mouseDoubleClickEvent(ev);
    auto index = selectedIndexes().first();

    auto node = index.isValid()
                ? static_cast<iscore::StateNode*>(index.internalPointer())
                : nullptr;

    if(node->is<iscore::MessageList>())
    {
        MessageListEditor ed(node->get<iscore::MessageList>(), this);
        int ret = ed.exec();

    }

}
