#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

#include <QLabel>

namespace Automation
{
class ProcessState;
// TODO this should not be some random widget but a
// ProcessStateInspectorDelegate
class StateInspectorWidget final : public Inspector::InspectorWidgetBase
{
public:
  explicit StateInspectorWidget(
      const ProcessState& object, const score::DocumentContext& context,
      QWidget* parent = nullptr);

private:
  void on_stateChanged();

  const ProcessState& m_state;
  QLabel* m_label{};
};
}
