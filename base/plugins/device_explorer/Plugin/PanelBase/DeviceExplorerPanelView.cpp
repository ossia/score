#include "DeviceExplorerPanelView.hpp"
#include "Panel/DeviceExplorerWidget.hpp"

#include <core/view/View.hpp>

DeviceExplorerPanelView::DeviceExplorerPanelView(iscore::View* parent) :
    iscore::PanelViewInterface {parent},
    m_widget {new DeviceExplorerWidget{parent}}
{
}

QWidget* DeviceExplorerPanelView::getWidget()
{
    return m_widget;
}

Qt::DockWidgetArea DeviceExplorerPanelView::defaultDock() const
{
    return Qt::LeftDockWidgetArea;
}

int DeviceExplorerPanelView::priority() const
{
    return 10;
}

QString DeviceExplorerPanelView::prettyName() const
{
    return tr("Devices");
}
