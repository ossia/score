#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <Inspector/InspectorSectionWidget.hpp>
#include <list>
#include <vector>

class QLabel;
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
                QWidget* parent = 0);

        void addState(const StateModel& state);
        void removeState(const StateModel& state);
        void focusState(const StateModel* state);

        CommandDispatcher<>* commandDispatcher() const
        { return m_commandDispatcher; }

        iscore::SelectionDispatcher& selectionDispatcher() const
        { return *m_selectionDispatcher.get(); }

    signals:
        void expandEventSection(bool b);

    private:
        void updateDisplayedValues();
        void on_conditionChanged();

        std::list<QWidget*> m_properties;

        std::map<Id<StateModel>, Inspector::InspectorSectionWidget*> m_statesSections;
        std::vector<QWidget*> m_states;

        //QLineEdit* m_stateLineEdit{};
        QWidget* m_statesWidget{};
        const EventModel& m_model;
        const iscore::DocumentContext& m_context;
        CommandDispatcher<>* m_commandDispatcher{};
        std::unique_ptr<iscore::SelectionDispatcher> m_selectionDispatcher;

        MetadataWidget* m_metadata {};

        ExpressionEditorWidget* m_exprEditor{};
};
}
