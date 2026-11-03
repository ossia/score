#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>

#include <score_plugin_dataflow_export.h>

namespace Dataflow
{
class ConnectionsWidget;
class SCORE_PLUGIN_DATAFLOW_EXPORT ConnectionsPanel final : public score::PanelDelegate
{
public:
  explicit ConnectionsPanel(const score::GUIApplicationContext& ctx);
  ~ConnectionsPanel() override;

  ConnectionsWidget& connections() const noexcept { return *m_widget; }

private:
  QWidget* widget() override;
  const score::PanelStatus& defaultPanelStatus() const override;
  void on_modelChanged(score::MaybeDocument oldm, score::MaybeDocument newm) override;
  void setNewSelection(const Selection& s) override;

  ConnectionsWidget* m_widget{};
};

class ConnectionsPanelFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("4a0b6f53-2d8e-4c1a-9a6e-0f5d7c3e9b21")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override;
};
}
