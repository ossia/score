#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <iscore/plugins/customfactory/UuidKey.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <list>

namespace Scenario
{
class StateModel;
class StateInspectorWidget final :
    public Inspector::InspectorWidgetBase
{
public:
  explicit StateInspectorWidget(
      const StateModel& object,
      const iscore::DocumentContext& context,
      QWidget* parent);

private:
  void splitFromEvent();
  void splitFromNode();
  void updateDisplayedValues();

  const StateModel& m_model;
  const iscore::DocumentContext& m_context;
  CommandDispatcher<> m_commandDispatcher;
  iscore::SelectionDispatcher m_selectionDispatcher;

  std::vector<QWidget*> m_properties;
};
}
