#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
class EventModel;

class QFormLayout;
class StateModel;
class MetadataWidget;
struct Message;
namespace iscore
{
class State;
}

/*!
 * \brief The EventInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an Event (Timebox) element.
 */
class EventInspectorWidget : public InspectorWidgetBase
{
        Q_OBJECT
    public:
        explicit EventInspectorWidget(
                const EventModel* object,
                QWidget* parent = 0);

        void addState(const StateModel* state);
        void focusState(const StateModel* state);
    signals:

    public slots:

        void updateDisplayedValues(const EventModel* obj);

        void on_addAddressClicked();
        void on_conditionChanged();
        void on_triggerChanged();

        void removeState(const iscore::State&);

        void updateInspector();
        void modelDateChanged();

    private:
        QVector<QWidget*> m_properties;

        std::vector<QWidget*> m_states;

        QLabel* m_date {};
        QLineEdit* m_conditionLineEdit{};
        QLineEdit* m_triggerLineEdit{};
        QLineEdit* m_stateLineEdit{};
        QWidget* m_statesWidget{};
        const EventModel* m_model{};

        InspectorSectionWidget* m_prevConstraints;
        InspectorSectionWidget* m_nextConstraints;

        MetadataWidget* m_metadata {};
};
