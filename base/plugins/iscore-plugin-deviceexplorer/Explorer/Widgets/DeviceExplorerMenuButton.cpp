#include <Device/QMenuView/qmenuview.h>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <qgridlayout.h>
#include <qlayout.h>
#include <qpushbutton.h>

#include "Device/Node/DeviceNode.hpp"
#include "DeviceExplorerMenuButton.hpp"

class QAbstractItemModel;
class QModelIndex;

DeviceExplorerMenuButton::DeviceExplorerMenuButton(DeviceExplorerModel *model)
{
    auto lay = new QGridLayout;
    this->setLayout(lay);
    lay->setSpacing(0);
    lay->setContentsMargins(0,0,0,0);

    // Menu button
    auto pb = new QPushButton {"/", this};

    auto menuview = new QMenuView {pb};
    menuview->setModel(reinterpret_cast<QAbstractItemModel*>(model));

    connect(menuview, &QMenuView::triggered,
            this, [&](const QModelIndex & m)
    { emit addressChosen(iscore::address(model->nodeFromModelIndex(m))); });

    pb->setMenu(menuview);
    this->layout()->addWidget(pb);
}
