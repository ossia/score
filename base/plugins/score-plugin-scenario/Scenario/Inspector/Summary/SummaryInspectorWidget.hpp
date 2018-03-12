#pragma once
#include <Inspector/InspectorWidgetBase.hpp>
#include <list>
#include <set>
#include <vector>

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
    Q_OBJECT
public:
  SummaryInspectorWidget(
      const IdentifiedObjectAbstract* obj,
      const std::set<const IntervalModel*>& intervals,
      const std::set<const TimeSyncModel*>& timesyncs,
      const std::set<const EventModel*>& events,
      const std::set<const StateModel*>& states,
      const score::DocumentContext& context,
      QWidget* parent = nullptr);
  ~SummaryInspectorWidget() override;

  QString tabName() override;

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
