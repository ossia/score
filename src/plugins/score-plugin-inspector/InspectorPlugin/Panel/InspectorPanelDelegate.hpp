#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
class QSpacerItem;
namespace InspectorPanel
{
class SelectionStackWidget;
class InspectorPanelWidget;

class PanelDelegate final : public score::PanelDelegate
{
public:
  PanelDelegate(const score::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;

  const score::PanelStatus& defaultPanelStatus() const override;

  void on_modelChanged(
      score::MaybeDocument oldm, score::MaybeDocument newm) override;
  void setNewSelection(const Selection& s) override;

  QWidget* m_widget{};
  InspectorPanelWidget* m_inspectorPanel{};
};
}
