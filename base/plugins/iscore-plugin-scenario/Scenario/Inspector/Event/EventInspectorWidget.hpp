#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
class EventModel;

class QFormLayout;
class StateModel;
class MetadataWidget;
struct Message;
class TriggerInspectorWidget;
class SimpleExpressionEditorWidget;

#include <QLineEdit>


/*!
 * \brief The EventInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an Event (Timebox) element.
 */
class EventInspectorWidget final : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit EventInspectorWidget(
                const EventModel& object,
                iscore::Document& doc,
                QWidget* parent = 0);

        void addState(const StateModel& state);
        void removeState(const StateModel& state);
        void focusState(const StateModel* state);

    public slots:
        void updateDisplayedValues();

        void on_conditionChanged();

        void modelDateChanged();

    private:
        std::list<QWidget*> m_properties;

        std::vector<QWidget*> m_states;

        QLabel* m_date {};
        QLineEdit* m_stateLineEdit{};
        QWidget* m_statesWidget{};
        const EventModel& m_model;

        MetadataWidget* m_metadata {};

        TriggerInspectorWidget* m_triggerWidg{};

        SimpleExpressionEditorWidget* m_exprEditor{};
};
