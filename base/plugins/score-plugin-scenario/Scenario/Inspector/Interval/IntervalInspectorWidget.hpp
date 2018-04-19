#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <QSlider>
#include <QString>
#include <QVector>
#include <Scenario/Inspector/Interval/IntervalInspectorDelegate.hpp>
#include <list>
#include <memory>
#include <nano_signal_slot.hpp>
#include <score/model/Identifier.hpp>
#include <score/tools/std/HashMap.hpp>
#include <score_plugin_scenario_export.h>
#include <vector>
namespace Inspector
{
class InspectorSectionWidget;
class InspectorWidgetList;
}
class QObject;
class QWidget;

namespace Scenario
{
class MetadataWidget;
class IntervalModel;
class ScenarioInterface;

/*!
 * \brief The IntervalInspectorWidget class
 *
 * Inherits from InspectorWidgetInterface. Manages an inteface for an
 * Interval (Timerack) element.
 */
class SCORE_PLUGIN_SCENARIO_EXPORT IntervalInspectorWidget final
    : public Inspector::InspectorWidgetBase
{
public:
  explicit IntervalInspectorWidget(
      const Inspector::InspectorWidgetList& list,
      const IntervalModel& object,
      std::unique_ptr<IntervalInspectorDelegate> del,
      const score::DocumentContext& context,
      QWidget* parent = nullptr);

  ~IntervalInspectorWidget();

  IntervalModel& model() const;

  const Inspector::InspectorWidgetList& widgetList() const
  {
    return m_widgetList;
  }

private:
  QString tabName() override;
  void updateDisplayedValues();

  // These methods are used to display created things
  QWidget* makeStatesWidget(Scenario::ScenarioInterface*);

  const Inspector::InspectorWidgetList& m_widgetList;
  const IntervalModel& m_model;

  QWidget* m_durationSection{};
  std::vector<QWidget*> m_properties;
  MetadataWidget* m_metadata{};
  std::unique_ptr<IntervalInspectorDelegate> m_delegate;
};
}
