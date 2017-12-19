#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score_plugin_deviceexplorer_export.h>

namespace Explorer
{
class DeviceExplorerWidget;
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT PanelDelegate final : public score::PanelDelegate
{
public:
  PanelDelegate(const score::GUIApplicationContext& ctx);

  QWidget* widget() override;

private:
  const score::PanelStatus& defaultPanelStatus() const override;

  void on_modelChanged(
      score::MaybeDocument oldm, score::MaybeDocument newm) override;

  DeviceExplorerWidget* m_widget{};
};
}
