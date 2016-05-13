#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>
#include <core/view/View.hpp>
#include <qnamespace.h>
#include <QObject>

#include "DeviceExplorerPanelView.hpp"

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/panel/PanelView.hpp>

class QWidget;

namespace Explorer
{
static const iscore::DefaultPanelStatus status{
    true,
    Qt::LeftDockWidgetArea,
            10,
            QObject::tr("Device Explorer")};

const iscore::DefaultPanelStatus &DeviceExplorerPanelView::defaultPanelStatus() const
{ return status; }

DeviceExplorerPanelView::DeviceExplorerPanelView(
        const iscore::ApplicationContext& ctx,
        QObject* parent) :
    iscore::PanelView {parent},
    m_widget {new DeviceExplorerWidget{
              ctx.components.factory<Device::DynamicProtocolList>(),
              nullptr}}
{
}

QWidget* DeviceExplorerPanelView::getWidget()
{
    return m_widget;
}
}
