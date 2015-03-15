#include "AutomationStateInspector.hpp"
#include "State/AutomationState.hpp"
#include <QLabel>
#include <QHBoxLayout>

AutomationStateInspector::AutomationStateInspector(AutomationState* object, QWidget* parent):
    InspectorWidgetBase{nullptr, parent}, // NOTE: this will crash if trying to send commands
    m_state{object},
    m_label{new QLabel}
{
    QVector<QWidget*> vec;
    vec.push_back(m_label);


    connect(m_state, &ProcessStateDataInterface::stateChanged,
            this,    &AutomationStateInspector::on_stateChanged);

    on_stateChanged();

    updateSectionsView(static_cast<QVBoxLayout*>(layout()), vec);
}

void AutomationStateInspector::on_stateChanged()
{
    qDebug() << Q_FUNC_INFO;
    QString text = m_state->point() == 0. ? tr("Start state: ") : tr("End state: ");
    m_label->setText(text + m_state->message().toString());
}
