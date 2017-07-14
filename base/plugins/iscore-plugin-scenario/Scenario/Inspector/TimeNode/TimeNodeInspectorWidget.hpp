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

private:
  QString tabName() override;

  void updateDisplayedValues();
  void on_dateChanged(const TimeVal&);

  const TimeNodeModel& m_model;

  MetadataWidget* m_metadata{};
  QLabel* m_date{};
  TriggerInspectorWidget* m_trigwidg{};
};
}
