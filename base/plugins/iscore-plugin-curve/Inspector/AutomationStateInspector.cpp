#include "AutomationStateInspector.hpp"
#include "State/AutomationState.hpp"
#include <QLabel>
#include <QHBoxLayout>

AutomationStateInspector::AutomationStateInspector(
        const AutomationState& object,
        QWidget* parent):
    InspectorWidgetBase{object, parent}, // NOTE: this will crash if trying to send commands
    m_state{object},
    m_label{new QLabel}
{
    QVector<QWidget*> vec;
    vec.push_back(m_label);


    connect(&m_state, &DynamicStateDataInterface::stateChanged,
            this,    &AutomationStateInspector::on_stateChanged);

    on_stateChanged();

    updateSectionsView(static_cast<QVBoxLayout*>(layout()), vec);
}

void AutomationStateInspector::on_stateChanged()
{
    m_label->setText(m_state.message().toString());
}
