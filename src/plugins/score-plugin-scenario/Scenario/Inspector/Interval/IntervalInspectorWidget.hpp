#pragma once

#include <Inspector/InspectorWidgetBase.hpp>

#include <score/model/Identifier.hpp>
#include <score/tools/std/HashMap.hpp>

#include <nano_signal_slot.hpp>
#include <score_plugin_scenario_export.h>

#include <list>
#include <memory>
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
      const score::DocumentContext& context,
      QWidget* parent = nullptr);

  ~IntervalInspectorWidget() override;

  IntervalModel& model() const;

private:
  const IntervalModel& m_model;
};
}
