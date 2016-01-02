#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <list>
#include <vector>

class EventModel;
class ExpressionEditorWidget;
class MetadataWidget;
class QLabel;
class QLineEdit;
class QWidget;
class StateModel;
class TriggerInspectorWidget;
namespace iscore {
class Document;
}  // namespace iscore


/*!
 * \brief The EventInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an Event (Timebox) element.
 */
class EventInspectorWidget final : public InspectorWidgetBase
{
    public:
        explicit EventInspectorWidget(
                const EventModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent = 0);

        void addState(const StateModel& state);
        void removeState(const StateModel& state);
        void focusState(const StateModel* state);

    private:
        void updateDisplayedValues();
        void on_conditionChanged();
        void modelDateChanged();


        std::list<QWidget*> m_properties;

        std::vector<QWidget*> m_states;

        QLabel* m_date {};
        //QLineEdit* m_stateLineEdit{};
        QWidget* m_statesWidget{};
        const EventModel& m_model;

        MetadataWidget* m_metadata {};

        TriggerInspectorWidget* m_triggerWidg{};

        ExpressionEditorWidget* m_exprEditor{};
};
