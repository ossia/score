#include "LibraryWidget.hpp"
#include <QStandardItemModel>
#include <QVBoxLayout>
LibraryWidget::LibraryWidget(QWidget* parent):
    QWidget{parent}
{
    auto lay = new QVBoxLayout;
    lay->setMargin(0);
    lay->setContentsMargins(0, 0, 0, 0);

    this->setLayout(lay);

    lay->addWidget(&m_tbl);
    m_tbl.setModel(new JSONModel);
    m_tbl.setDragEnabled(true);
    m_tbl.setAcceptDrops(true);
    m_tbl.setDropIndicatorShown(true);
}
