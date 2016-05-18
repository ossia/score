#include "UndoPanelDelegate.hpp"

#include <core/undo/Panel/Widgets/UndoListWidget.hpp>
#include <core/document/Document.hpp>
#include <QVBoxLayout>

namespace iscore
{
UndoPanelDelegate::UndoPanelDelegate(
        const ApplicationContext &ctx):
    PanelDelegate{ctx},
    m_widget{new QWidget}
{
    m_widget->setLayout(new QVBoxLayout);
    m_widget->setObjectName("HistoryExplorer");
}

QWidget *UndoPanelDelegate::widget()
{
    return m_widget;
}

const PanelStatus &UndoPanelDelegate::defaultPanelStatus() const
{
    static const iscore::PanelStatus status{
        true,
        Qt::LeftDockWidgetArea,
                1,
                QObject::tr("History"),
                QKeySequence::fromString("Ctrl+H")};

    return status;
}

void UndoPanelDelegate::on_modelChanged(
        PanelDelegate::maybe_document_t oldm,
        PanelDelegate::maybe_document_t newm)
{
    delete m_list;
    m_list = nullptr;

    if (newm)
    {
        m_list = new iscore::UndoListWidget{
                (*newm).document.commandStack()};
        m_widget->layout()->addWidget(m_list);
    }
}

}
