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
namespace Process
{
class ProcessModel;
class ProcessFactoryList;
}
class QObject;
class QWidget;

namespace Scenario
{
class MetadataWidget;
class ConstraintModel;
class ProcessModel;
class ScenarioInterface;
class ProcessTabWidget;
class ProcessViewTabWidget;

/*!
 * \brief The ConstraintInspectorWidget class
 *
 * Inherits from InspectorWidgetInterface. Manages an inteface for an
 * Constraint (Timerack) element.
 */
class ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintInspectorWidget final
    : public Inspector::InspectorWidgetBase,
      public Nano::Observer
{
public:
  explicit ConstraintInspectorWidget(
      const Inspector::InspectorWidgetList& list,
      const Process::ProcessFactoryList& pl,
      const ConstraintModel& object,
      std::unique_ptr<ConstraintInspectorDelegate>
          del,
      const iscore::DocumentContext& context,
      QWidget* parent = nullptr);

  ~ConstraintInspectorWidget();

  ConstraintModel& model() const;

  const Inspector::InspectorWidgetList& widgetList() const
  {
    return m_widgetList;
  }

  const Process::ProcessFactoryList& processList() const
  {
    return m_processList;
  }

private:
  QString tabName() override;

  void updateDisplayedValues();

  // Interface of Constraint

  // These methods are used to display created things

  void on_processCreated(const Process::ProcessModel&);
  void on_processRemoved(const Process::ProcessModel&);
  void on_orderChanged();

  QWidget* makeStatesWidget(Scenario::ScenarioInterface*);

  const Inspector::InspectorWidgetList& m_widgetList;
  const Process::ProcessFactoryList& m_processList;
  const ConstraintModel& m_model;

  // InspectorSectionWidget* m_eventsSection {};
  QWidget* m_durationSection{};

  Scenario::ProcessTabWidget* m_processesTabPage{};

  std::list<QWidget*> m_properties;

  MetadataWidget* m_metadata{};

  std::unique_ptr<ConstraintInspectorDelegate> m_delegate;
};
}
