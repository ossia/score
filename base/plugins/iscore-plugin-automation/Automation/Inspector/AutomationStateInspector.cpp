#include "AutomationStateInspector.hpp"
#include "Automation/State/AutomationState.hpp"
#include <QLabel>
#include <QHBoxLayout>

AutomationStateInspector::AutomationStateInspector(
        const AutomationState& object,
        iscore::Document& doc,
        QWidget* parent):
    InspectorWidgetBase{object, doc, parent},
    m_state{object},
    m_label{new QLabel}
{
    QVector<QWidget*> vec;
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
