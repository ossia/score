#include "TriggerModel.hpp"
#include <State/Expression.hpp>
#include <iscore/model/IdentifiedObject.hpp>

class QObject;
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
TriggerModel::TriggerModel(const Id<TriggerModel>& id, QObject* parent)
    : IdentifiedObject<TriggerModel>{
          id, Metadata<ObjectKey_k, TriggerModel>::get(), parent}
{
  m_expression.push_back(State::Expression{
      State::Relation{State::RelationMember{State::Value::fromValue(true)},
                      ossia::expressions::comparator::EQUAL,
                      State::RelationMember{State::Value::fromValue(false)}},
      &m_expression});
}

State::Expression TriggerModel::expression() const
{
  return m_expression;
}

void TriggerModel::setExpression(const State::Expression& expression)
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
