#pragma once
#include <Inspector/InspectorWidgetBase.hpp>
#include <list>
#include <set>
#include <vector>

namespace Scenario
{
class ConstraintModel;
class TimeNodeModel;
class EventModel;
class StateModel;
class SummaryInspectorWidget final : public Inspector::InspectorWidgetBase
{
public:
  SummaryInspectorWidget(
      const IdentifiedObjectAbstract* obj,
      const std::set<const ConstraintModel*>&
          constraints,
      const std::set<const TimeNodeModel*>&
          timenodes,
      const std::set<const EventModel*>&
          events,
      const std::set<const StateModel*>&
          states,
      const iscore::DocumentContext& context,
      QWidget* parent = nullptr);

  QString tabName() override;

private:
  std::vector<QWidget*> m_properties;
};
}
