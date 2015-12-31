#include <QLabel>
#include <list>
#include <QVBoxLayout>

#include <Automation/State/AutomationState.hpp>
#include "AutomationStateInspector.hpp"
#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/State/ProcessStateDataInterface.hpp>
#include <State/Message.hpp>
#include <iscore/tools/Todo.hpp>

class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Automation
{
StateInspectorWidget::StateInspectorWidget(
        const State& object,
        const iscore::DocumentContext& doc,
        QWidget* parent):
    InspectorWidgetBase{object, doc, parent},
    m_state{object},
    m_label{new QLabel}
{
    std::list<QWidget*> vec;
    vec.push_back(m_label);


    con(m_state, &ProcessStateDataInterface::stateChanged,
        this,    &StateInspectorWidget::on_stateChanged);

    on_stateChanged();

    updateSectionsView(safe_cast<QVBoxLayout*>(layout()), vec);
}

void StateInspectorWidget::on_stateChanged()
{
    m_label->setText(m_state.message().toString());
}
}
