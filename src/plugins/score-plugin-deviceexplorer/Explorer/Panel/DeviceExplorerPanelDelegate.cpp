// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceExplorerPanelDelegate.hpp"

#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>

#include <score/application/GUIApplicationContext.hpp>

namespace Explorer
{
PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_widget{new DeviceExplorerWidget{ctx.interfaces<Device::ProtocolFactoryList>(), nullptr}}

{
  m_widget->setStatusTip(
      QObject::tr("The device explorer displays and controls network devices "
                  "and hardware peripherals."));
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{
      true,
      false,
      Qt::LeftDockWidgetArea,
      100,
      QObject::tr("Device Explorer"),
      "device_explorer",
      QObject::tr("Ctrl+Shift+D")};

  return status;
}

void PanelDelegate::on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm)
{
  // DeviceExplorerModel ownership goes to document plugin
  if (oldm)
  {
    if (auto plug = oldm->findPlugin<DeviceDocumentPlugin>())
      plug->explorer().setView(nullptr);
  }

  if (newm)
  {
    if (auto plug = newm->findPlugin<DeviceDocumentPlugin>())
    {
      plug->explorer().setView(m_widget->view());
      m_widget->setModel(&plug->explorer());
    }
  }
  else
  {
    m_widget->setModel(nullptr);
  }
}
}
