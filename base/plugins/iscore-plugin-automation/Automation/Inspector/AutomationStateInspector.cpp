#include <QLabel>
#include <list>

#include <Automation/State/AutomationState.hpp>
#include "AutomationStateInspector.hpp"
#include "Inspector/InspectorWidgetBase.hpp"
#include <Process/State/ProcessStateDataInterface.hpp>
#include <State/Message.hpp>
#include <iscore/tools/Todo.hpp>

class QVBoxLayout;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

AutomationStateInspector::AutomationStateInspector(
        const AutomationState& object,
        iscore::Document& doc,
        QWidget* parent):
    InspectorWidgetBase{object, doc, parent},
    m_state{object},
    m_label{new QLabel}
{
    std::list<QWidget*> vec;
    vec.push_back(m_label);


    con(m_state, &ProcessStateDataInterface::stateChanged,
        this,    &AutomationStateInspector::on_stateChanged);

    on_stateChanged();

    updateSectionsView(safe_cast<QVBoxLayout*>(layout()), vec);
}

void AutomationStateInspector::on_stateChanged()
{
    m_label->setText(m_state.message().toString());
}
