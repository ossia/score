// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "AutomationStateInspector.hpp"

#include <Automation/State/AutomationState.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <State/Message.hpp>

#include <score/tools/Todo.hpp>
#include <score/widgets/TextLabel.hpp>

#include <QLabel>
#include <QVBoxLayout>

#include <list>

namespace Automation
{
StateInspectorWidget::StateInspectorWidget(
    const ProcessState& object,
    const score::DocumentContext& doc,
    QWidget* parent)
    : InspectorWidgetBase{object, doc, parent, tr("State")}
    , m_state{object}
    , m_label{new TextLabel}
{
  std::vector<QWidget*> vec;
  vec.push_back(m_label);

  con(m_state,
      &ProcessStateDataInterface::stateChanged,
      this,
      &StateInspectorWidget::on_stateChanged);

  on_stateChanged();

  updateSectionsView(safe_cast<QVBoxLayout*>(layout()), vec);
}

void StateInspectorWidget::on_stateChanged()
{
  m_label->setText(m_state.message().toString());
}
}
