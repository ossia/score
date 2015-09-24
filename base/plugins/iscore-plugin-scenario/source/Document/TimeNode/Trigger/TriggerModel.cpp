#include "TriggerModel.hpp"

TriggerModel::TriggerModel(QObject* parent):
    QObject{parent}
{

}


Trigger TriggerModel::expression() const
{
    return m_expression;
}

void TriggerModel::setExpression(const Trigger& expression)
{
    if (m_expression == expression)
        return;
    m_expression = expression;
    emit triggerChanged(m_expression);
}

bool TriggerModel::active() const
{
    return m_active;
}

void TriggerModel::setActive(bool active)
{
    if (active == m_active)
        return;
    m_active = active;
    emit activeChanged();
}
