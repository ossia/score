#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <Scenario/Inspector/Constraint/ConstraintInspectorDelegate.hpp>

#include <QString>
#include <QVector>
#include <list>
#include <memory>
#include <nano_signal_slot.hpp>
#include <iscore/tools/std/HashMap.hpp>
#include <vector>

#include <iscore/model/Identifier.hpp>
#include <iscore_plugin_scenario_export.h>
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
class ConstraintModel;
class ScenarioInterface;

/*!
 * \brief The ConstraintInspectorWidget class
 *
 * Inherits from InspectorWidgetInterface. Manages an inteface for an
 * Constraint (Timerack) element.
 */
class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintInspectorWidget final
    : public Inspector::InspectorWidgetBase
{
public:
  explicit ConstraintInspectorWidget(
      const Inspector::InspectorWidgetList& list,
      const ConstraintModel& object,
      std::unique_ptr<ConstraintInspectorDelegate> del,
      const iscore::DocumentContext& context,
      QWidget* parent = nullptr);

  ~ConstraintInspectorWidget();

  ConstraintModel& model() const;

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
  const ConstraintModel& m_model;

  QWidget* m_durationSection{};
  std::vector<QWidget*> m_properties;
  MetadataWidget* m_metadata{};
  std::unique_ptr<ConstraintInspectorDelegate> m_delegate;
};
}
