#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <iscore/plugins/customfactory/UuidKey.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <list>

namespace Inspector
{
class InspectorSectionWidget;
}
class QWidget;
namespace iscore
{
struct DocumentContext;
} // namespace iscore
namespace Process
{
class StateProcessFactory;
class StateProcess;
}
namespace Scenario
{
class AddStateProcessDialog;
class StateModel;
class StateInspectorWidget final :
    public Inspector::InspectorSectionWidget,
    public Nano::Observer
{
public:
  explicit StateInspectorWidget(
      const StateModel& object,
      const iscore::DocumentContext& context,
      QWidget* parent);

public slots:
  void splitEvent();

private:
  void on_stateProcessCreated(const Process::StateProcess&);
  void on_stateProcessRemoved(const Process::StateProcess&);
  void createStateProcess(const UuidKey<Process::StateProcessFactory>&);
  Inspector::InspectorSectionWidget*
  displayStateProcess(const Process::StateProcess& process);
  void updateDisplayedValues();

  const StateModel& m_model;
  const iscore::DocumentContext& m_context;
  CommandDispatcher<> m_commandDispatcher;
  iscore::SelectionDispatcher m_selectionDispatcher;

  AddStateProcessDialog* m_addProcess{};
};
}
