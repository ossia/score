#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
class EventModel;

class QFormLayout;
class StateModel;
class MetadataWidget;
struct Message;

#include <QLineEdit>
#include <QValidator>
#include <State/Expression.hpp>
class ExpressionValidator : public QValidator
{
    public:
        State validate(QString& str, int&) const
        {
            m_currentExp = iscore::parse(str);
            if(m_currentExp)
                return State::Acceptable;
            else
                return State::Invalid;
        }

        // Call only after a successful validate()
        iscore::Expression get() const
        { return *m_currentExp; }

    private:
        mutable boost::optional<iscore::Expression> m_currentExp;
};

/*!
 * \brief The EventInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an Event (Timebox) element.
 */
class EventInspectorWidget : public InspectorWidgetBase
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
        void on_triggerChanged();

        void modelDateChanged();

    private:
        QVector<QWidget*> m_properties;

        std::vector<QWidget*> m_states;

        QLabel* m_date {};
        QLineEdit* m_conditionLineEdit{};
        QLineEdit* m_triggerLineEdit{};
        QLineEdit* m_stateLineEdit{};
        QWidget* m_statesWidget{};
        const EventModel& m_model;

        MetadataWidget* m_metadata {};

        ExpressionValidator m_validator;
};
