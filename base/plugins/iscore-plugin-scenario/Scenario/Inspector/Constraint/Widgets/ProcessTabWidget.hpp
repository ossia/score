#pragma once

#include <QWidget>
#include <Scenario/Inspector/Constraint/ConstraintInspectorWidget.hpp>

namespace Inspector
{
class InspectorSectionWidget;
}
namespace Process
{
class ProcessModelFactory;
class LayerFactory;
}
namespace Scenario
{
class AddProcessDialog;
class ProcessTabWidget : public QWidget, public Nano::Observer
{
  Q_OBJECT
public:
  explicit ProcessTabWidget(
      const ConstraintInspectorWidget& parentCstr, QWidget* parent = nullptr);

signals:

public slots:
  void createProcess(const UuidKey<Process::ProcessModel>& processName);
  void displayProcess(const Process::ProcessModel&, bool expanded);

  void updateDisplayedValues();

private:
  void ask_processNameChanged(const Process::ProcessModel& p, QString s);
  void createLayerInNewSlot(const Id<Process::ProcessModel>& processId);

  const ConstraintInspectorWidget& m_constraintWidget;

  std::vector<Inspector::InspectorSectionWidget*> m_processesSectionWidgets;
  AddProcessDialog* m_addProcess{};
};
}
