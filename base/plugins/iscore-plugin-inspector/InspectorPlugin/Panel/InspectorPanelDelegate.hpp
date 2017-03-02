#pragma once
#include <iscore/plugins/panel/PanelDelegate.hpp>
class QSpacerItem;
namespace InspectorPanel
{
class SelectionStackWidget;
class InspectorPanelWidget;

class PanelDelegate final : public iscore::PanelDelegate
{
public:
  PanelDelegate(const iscore::GUIApplicationContext& ctx);

private:
  QWidget* widget() override;

  const iscore::PanelStatus& defaultPanelStatus() const override;

  void on_modelChanged(
      iscore::MaybeDocument oldm, iscore::MaybeDocument newm) override;
  void setNewSelection(const Selection& s) override;

  QWidget* m_widget{};
  SelectionStackWidget* m_stack{};
  InspectorPanelWidget* m_inspectorPanel{};
};
}
