#pragma once
#include <Inspector/InspectorWidgetBase.hpp>

#include <ossia/detail/hash_map.hpp>

#include <list>
#include <vector>
#include <verdigris>

namespace Inspector
{
class InspectorSectionWidget;
}
namespace Scenario
{
class TimeSyncSummaryWidget;
class EventSummaryWidget;
class IntervalSummaryWidget;
class IntervalModel;
class TimeSyncModel;
class EventModel;
class StateModel;
class SummaryInspectorWidget final : public Inspector::InspectorWidgetBase
{
  W_OBJECT(SummaryInspectorWidget)
public:
  SummaryInspectorWidget(
      const IdentifiedObjectAbstract* obj,
      const ossia::hash_set<const IntervalModel*>& intervals,
      const ossia::hash_set<const TimeSyncModel*>& timesyncs,
      const ossia::hash_set<const EventModel*>& events,
      const ossia::hash_set<const StateModel*>& states,
      const score::DocumentContext& context, QWidget* parent = nullptr);
  ~SummaryInspectorWidget() override;

  void update(const QList<const IdentifiedObjectAbstract*>&);

private:
  std::vector<QWidget*> m_properties;
  Inspector::InspectorSectionWidget* m_itvSection{};
  Inspector::InspectorSectionWidget* m_syncSection{};
  Inspector::InspectorSectionWidget* m_evSection{};

  std::vector<IntervalSummaryWidget*> m_itvs;
  std::vector<EventSummaryWidget*> m_evs;
  std::vector<TimeSyncSummaryWidget*> m_syncs;
};
}
