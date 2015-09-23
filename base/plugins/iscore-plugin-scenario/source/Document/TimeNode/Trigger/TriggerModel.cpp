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
