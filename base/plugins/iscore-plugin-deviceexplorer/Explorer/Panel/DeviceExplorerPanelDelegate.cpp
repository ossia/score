#include "DeviceExplorerPanelDelegate.hpp"
#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>
#include <iscore/application/GUIApplicationContext.hpp>

namespace Explorer
{
PanelDelegate::PanelDelegate(const iscore::GUIApplicationContext& ctx)
    : iscore::PanelDelegate{ctx}
    , m_widget{new DeviceExplorerWidget{
          ctx.interfaces<Device::ProtocolFactoryList>(), nullptr}}
{
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const iscore::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const iscore::PanelStatus status{true, Qt::LeftDockWidgetArea, 10,
                                          QObject::tr("Device Explorer"),
                                          QObject::tr("Ctrl+Shift+E")};

  return status;
}

void PanelDelegate::on_modelChanged(
    iscore::MaybeDocument oldm, iscore::MaybeDocument newm)
{
  // DeviceExplorerModel ownership goes to document plugin
  if (oldm)
  {
    auto& plug = oldm->plugin<DeviceDocumentPlugin>();
    plug.explorer().setView(nullptr);
  }

  if (newm)
  {
    auto& plug = newm->plugin<DeviceDocumentPlugin>();
    plug.explorer().setView(m_widget->view());
    m_widget->setModel(&plug.explorer());
  }
  else
  {
    m_widget->setModel(nullptr);
  }
}
}
