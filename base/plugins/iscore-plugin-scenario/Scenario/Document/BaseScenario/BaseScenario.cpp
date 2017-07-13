// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <QString>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/Trigger/TriggerModel.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <tuple>

#include "BaseScenario.hpp"
#include <ossia/detail/algorithms.hpp>
#include <Scenario/Document/BaseScenario/BaseScenarioContainer.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/model/Identifier.hpp>

class QObject;

namespace Scenario
{
BaseScenario::BaseScenario(const Id<BaseScenario>& id, QObject* parent)
    : IdentifiedObject<BaseScenario>{id, "Scenario::BaseScenario", parent}
    , BaseScenarioContainer{this}
{
  m_endNode->trigger()->setActive(true);
}

BaseScenario::~BaseScenario()
{
}

Selection BaseScenario::selectedChildren() const
{
  Selection s;
  ossia::for_each_in_tuple(elements(), [&](auto elt) {
    if (elt->selection.get())
      s.append(elt);
  });
  return s;
}

bool BaseScenario::focused() const
{
  bool res = false;
  ossia::for_each_in_tuple(elements(), [&](auto elt) {
    if (elt->selection.get())
    {
      res = true;
    }
  });

  return res;
}

const QVector<Id<ConstraintModel>> constraintsBeforeTimeNode(
    const BaseScenario& scen, const Id<TimeNodeModel>& timeNodeId)
{
  if (timeNodeId == scen.endTimeNode().id())
  {
    return {scen.constraint().id()};
  }
  return {};
}
}
