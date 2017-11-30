#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
namespace Explorer
{
class DeviceExplorerWidget;
class PanelDelegate final : public score::PanelDelegate
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
