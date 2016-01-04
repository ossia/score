#include "ProcessPanelView.hpp"

#include <iscore/widgets/ClearLayout.hpp>
#include <QVBoxLayout>
#include <QWidget>


ProcessPanelView::ProcessPanelView(QObject* parent):
    iscore::PanelView{parent}
{
    m_widget = new QWidget;
    m_lay = new QVBoxLayout;
    m_widget->setLayout(m_lay);
}

void ProcessPanelView::setInnerWidget(QWidget* widg)
{
    iscore::clearLayout(m_lay);
    m_lay->addWidget(widg);
}

QWidget* ProcessPanelView::getWidget()
{
    return m_widget;
}

static const iscore::DefaultPanelStatus status{false, Qt::BottomDockWidgetArea, 10, QObject::tr("Process")};
const iscore::DefaultPanelStatus &ProcessPanelView::defaultPanelStatus() const
{
    return status;
}
