#pragma once
#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <score/plugins/customfactory/UuidKey.hpp>
#include <score/selection/SelectionDispatcher.hpp>
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
      const score::DocumentContext& context,
      QWidget* parent);

private:
  QString tabName() override;

  void splitFromEvent();
  void splitFromNode();
  void updateDisplayedValues();

  const StateModel& m_model;
  const score::DocumentContext& m_context;
  CommandDispatcher<> m_commandDispatcher;

  std::vector<QWidget*> m_properties;
};
}
