#include "DeviceExplorerPanelView.hpp"
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>

#include <core/view/View.hpp>
#include <core/presenter/Presenter.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <core/application/Application.hpp>
static const iscore::DefaultPanelStatus status{
    true,
    Qt::LeftDockWidgetArea,
            10,
            QObject::tr("Devices")};

const iscore::DefaultPanelStatus &DeviceExplorerPanelView::defaultPanelStatus() const
{ return status; }

DeviceExplorerPanelView::DeviceExplorerPanelView(
        const iscore::ApplicationContext& ctx,
        iscore::View* parent) :
    iscore::PanelView {parent},
    m_widget {new DeviceExplorerWidget{
              ctx.components.factory<DynamicProtocolList>(),
              parent}}
{
}

QWidget* DeviceExplorerPanelView::getWidget()
{
    return m_widget;
}
