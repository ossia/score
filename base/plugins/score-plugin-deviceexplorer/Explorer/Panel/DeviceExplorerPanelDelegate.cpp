// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "DeviceExplorerPanelDelegate.hpp"
#include <Device/Protocol/ProtocolList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerWidget.hpp>
#include <score/application/GUIApplicationContext.hpp>

namespace Explorer
{
PanelDelegate::PanelDelegate(const score::GUIApplicationContext& ctx)
    : score::PanelDelegate{ctx}
    , m_widget{new DeviceExplorerWidget{
          ctx.interfaces<Device::ProtocolFactoryList>(), nullptr}}

{
}

QWidget* PanelDelegate::widget()
{
  return m_widget;
}

const score::PanelStatus& PanelDelegate::defaultPanelStatus() const
{
  static const score::PanelStatus status{true, Qt::LeftDockWidgetArea, 10,
                                          QObject::tr("Device Explorer"),
                                          QObject::tr("Ctrl+Shift+E")};

  return status;
}

void PanelDelegate::on_modelChanged(
    score::MaybeDocument oldm, score::MaybeDocument newm)
{
#if !defined(__EMSCRIPTEN__)
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
#endif
}
}
