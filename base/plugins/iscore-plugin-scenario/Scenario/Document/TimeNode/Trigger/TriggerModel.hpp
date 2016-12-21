#pragma once
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <State/Expression.hpp>
#include <iscore/model/IdentifiedObject.hpp>
#include <iscore/tools/Metadata.hpp>

class QObject;
#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class ISCORE_PLUGIN_SCENARIO_EXPORT TriggerModel final
    : public IdentifiedObject<TriggerModel>
{
  Q_OBJECT

public:
  TriggerModel(const Id<TriggerModel>& id, QObject* parent);

  State::Expression expression() const;
  void setExpression(const State::Expression& expression);

  bool active() const;
  void setActive(bool active);

  // Note : this is for API -> UI communication.
  // To trigger by hand we have the triggered() signal.
  ExecutionStatusProperty executionStatus; // TODO serialize me ?

signals:
  void triggerChanged(const State::Expression&);
  void activeChanged();

  void triggeredByGui() const;

private:
  State::Expression m_expression;
  bool m_active{false};
};
}

DEFAULT_MODEL_METADATA(Scenario::TriggerModel, "Trigger")
