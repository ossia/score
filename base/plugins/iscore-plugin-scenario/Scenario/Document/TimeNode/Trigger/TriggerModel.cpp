#include <State/Expression.hpp>
#include "TriggerModel.hpp"
#include <iscore/tools/IdentifiedObject.hpp>

class QObject;
template <typename tag, typename impl> class id_base_t;

constexpr const char TriggerModel::className[];
TriggerModel::TriggerModel(const Id<TriggerModel>& id, QObject* parent):
    IdentifiedObject<TriggerModel>{id, className, parent}
{

}

iscore::Trigger TriggerModel::expression() const
{
    return m_expression;
}

void TriggerModel::setExpression(const iscore::Trigger& expression)
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
