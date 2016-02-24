#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
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
class EventInspectorWidget final : public Inspector::InspectorWidgetBase
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
        QString tabName() override;

        void updateDisplayedValues();
        void on_conditionChanged();


        std::list<QWidget*> m_properties;

        std::vector<QWidget*> m_states;

        //QLineEdit* m_stateLineEdit{};
        QWidget* m_statesWidget{};
        const EventModel& m_model;

        MetadataWidget* m_metadata {};

        ExpressionEditorWidget* m_exprEditor{};
};
}
