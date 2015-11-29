#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>
#include <core/view/View.hpp>
#include <qnamespace.h>
#include <QObject>

#include "DeviceExplorerPanelView.hpp"
#include <core/application/ApplicationComponents.hpp>
#include <core/application/ApplicationContext.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/plugins/panel/PanelView.hpp>

class QWidget;

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
