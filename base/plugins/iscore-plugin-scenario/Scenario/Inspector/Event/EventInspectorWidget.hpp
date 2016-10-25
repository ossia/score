#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <Inspector/InspectorSectionWidget.hpp>
#include <iscore/selection/SelectionDispatcher.hpp>
#include <list>
#include <vector>
#include <Scenario/Inspector/Expression/ExpressionMenu.hpp>

class QLabel;
class QComboBox;
class QLineEdit;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore


namespace Scenario
{
class StateModel;
class EventModel;
class ExpressionEditorWidget;
class MetadataWidget;
class TriggerInspectorWidget;
/*!
 * \brief The EventInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an Event (Timebox) element.
 */
class EventInspectorWidget final : public QWidget
{
        Q_OBJECT
    public:
        explicit EventInspectorWidget(
                const EventModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent = nullptr);

        void addState(const StateModel& state);
        //void removeState(const Id<StateModel>& state);
        void focusState(const StateModel* state);

    signals:
        void expandEventSection(bool b);

    private:
        void updateDisplayedValues();
        void on_conditionChanged();
        void on_conditionReset();

        std::list<QWidget*> m_properties;

        std::map<Id<StateModel>, Inspector::InspectorSectionWidget*> m_statesSections;
        std::vector<QWidget*> m_states;

        //QLineEdit* m_stateLineEdit{};
        QWidget* m_statesWidget{};
        const EventModel& m_model;
        const iscore::DocumentContext& m_context;
        CommandDispatcher<> m_commandDispatcher;
        iscore::SelectionDispatcher m_selectionDispatcher;

        MetadataWidget* m_metadata {};

        ExpressionMenu m_menu;
        ExpressionEditorWidget* m_exprEditor{};
        QComboBox* m_offsetBehavior{};
};
}
