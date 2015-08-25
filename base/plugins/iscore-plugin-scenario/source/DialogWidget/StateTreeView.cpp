#include "StateTreeView.hpp"

#include <DeviceExplorer/../Plugin/Widgets/MessageWidget.hpp>
#include <State/StateItemModel.hpp>
StateTreeView::StateTreeView(
        iscore::StateItemModel* model,
        DeviceExplorerModel* devexplorer,
        QWidget* parent):
    QTreeView{parent},
    m_devExplorer{devexplorer}
{
    this->setModel(model);
}

void StateTreeView::mouseDoubleClickEvent(QMouseEvent* ev)
{
    QTreeView::mouseDoubleClickEvent(ev);
    auto index = selectedIndexes().first();

    auto node = index.isValid()
                ? static_cast<iscore::StateNode*>(index.internalPointer())
                : nullptr;

    if(node->is<iscore::MessageList>())
    {
        MessageListEditor ed(node->get<iscore::MessageList>(), m_devExplorer, this);
        int ret = ed.exec();
    }

}
