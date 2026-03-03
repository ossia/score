#pragma once
#include <Inspector/InspectorWidgetBase.hpp>

class QComboBox;
class QDoubleSpinBox;

namespace ClipLauncher
{
class CellModel;
class ProcessModel;

class CellInspectorWidget final : public Inspector::InspectorWidgetBase
{
public:
  CellInspectorWidget(
      const CellModel& cell, const score::DocumentContext& ctx, QWidget* parent);
  ~CellInspectorWidget();

private:
  void rebuildTransitionRulesList();
  const ProcessModel& parentProcess() const;

  const CellModel& m_cell;
  const score::DocumentContext& m_ctx;
  QComboBox* m_launchModeCombo{};
  QComboBox* m_triggerStyleCombo{};
  QDoubleSpinBox* m_velocitySpin{};
  QWidget* m_transitionRulesContainer{};
};

} // namespace ClipLauncher
