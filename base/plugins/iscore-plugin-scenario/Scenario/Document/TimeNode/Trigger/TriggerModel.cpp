#include <State/Expression.hpp>
#include "TriggerModel.hpp"
#include <iscore/tools/IdentifiedObject.hpp>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

ISCORE_METADATA_IMPL(Scenario::TriggerModel)
namespace Scenario
{
TriggerModel::TriggerModel(const Id<TriggerModel>& id, QObject* parent):
    IdentifiedObject<TriggerModel>{id, className.c_str(), parent}
{

}

State::Trigger TriggerModel::expression() const
{
    return m_expression;
}

void TriggerModel::setExpression(const State::Trigger& expression)
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
}
