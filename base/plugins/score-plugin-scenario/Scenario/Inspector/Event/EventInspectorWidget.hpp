#pragma once

#include <Inspector/InspectorSectionWidget.hpp>
#include <Inspector/InspectorWidgetBase.hpp>
#include <Scenario/Inspector/Expression/ExpressionMenu.hpp>
#include <score/selection/SelectionDispatcher.hpp>
#include <list>
#include <vector>

class QLabel;
class QComboBox;
class QLineEdit;
class QWidget;
namespace Scenario
{
class StateModel;
class EventModel;
class ExpressionEditorWidget;
class MetadataWidget;
class TriggerInspectorWidget;
/*!
 * \brief The EventInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an
 * Event (Timebox) element.
 */
class EventInspectorWidget final : public Inspector::InspectorWidgetBase
{
  Q_OBJECT
public:
  explicit EventInspectorWidget(
      const EventModel& object,
      const score::DocumentContext& context,
      QWidget* parent = nullptr);

Q_SIGNALS:
  void expandEventSection(bool b);

private:
  QString tabName() override;

  void updateDisplayedValues();
  void on_conditionChanged();
  void on_conditionReset();

  std::vector<QWidget*> m_properties;

  QPointer<const EventModel> m_model;
  const score::DocumentContext& m_context;
  CommandDispatcher<> m_commandDispatcher;
  score::SelectionDispatcher m_selectionDispatcher;

  MetadataWidget* m_metadata{};

  ExpressionMenu m_menu;
  ExpressionEditorWidget* m_exprEditor{};
  QComboBox* m_offsetBehavior{};
};
}
