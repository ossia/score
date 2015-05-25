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


}

void LibraryWidget::setLibrary(JSONLibrary* lib)
{
    m_model.clear();
    m_model.setHorizontalHeaderLabels({tr("Name"), tr("Tags")});
    for(auto& element : lib->elements)
    {
        auto first = new QStandardItem(element.name);
        first->setSelectable(false);
        auto second = new QStandardItem(element.tags.join(", "));
        second->setSelectable(false);

        m_model.appendRow({first, second});
    }
    m_tbl.setModel(&m_model);
}
