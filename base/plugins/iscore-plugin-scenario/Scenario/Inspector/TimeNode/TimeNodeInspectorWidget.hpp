#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <Process/TimeValue.hpp>
#include <vector>

namespace Inspector
{
class InspectorSectionWidget;
}
class QLabel;
namespace Scenario
{
class MetadataWidget;
class TriggerInspectorWidget;
class EventModel;
class TimeNodeModel;
/*!
 * \brief The TimeNodeInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an
 * TimeNode (Timebox) element.
 */
class TimeNodeInspectorWidget final : public Inspector::InspectorWidgetBase
{
public:
  explicit TimeNodeInspectorWidget(
      const TimeNodeModel& object,
      const iscore::DocumentContext& context,
      QWidget* parent);

  void addEvent(const EventModel& event);
  void removeEvent(const Id<EventModel>& event);

private:
  QString tabName() override;

  void updateDisplayedValues();
  void on_dateChanged(const TimeVal&);

  std::vector<QWidget*> m_properties;
  QWidget* m_events{};

  const TimeNodeModel& m_model;

  std::map<const Id<EventModel>, Inspector::InspectorSectionWidget*>
      m_eventList{};
  QLabel* m_date{};

  MetadataWidget* m_metadata{};

  TriggerInspectorWidget* m_trigwidg{};
};
}
