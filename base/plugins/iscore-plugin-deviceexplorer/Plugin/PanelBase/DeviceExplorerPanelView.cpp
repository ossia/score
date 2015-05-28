#include "DeviceExplorerPanelView.hpp"
#include "Panel/DeviceExplorerWidget.hpp"

#include <core/view/View.hpp>
static const iscore::DefaultPanelStatus status{true, Qt::LeftDockWidgetArea, 10, QObject::tr("Devices")};

const iscore::DefaultPanelStatus &DeviceExplorerPanelView::defaultPanelStatus() const
{ return status; }

DeviceExplorerPanelView::DeviceExplorerPanelView(iscore::View* parent) :
    iscore::PanelView {parent},
    m_widget {new DeviceExplorerWidget{parent}}
{
}

QWidget* DeviceExplorerPanelView::getWidget()
{
    return m_widget;
}
