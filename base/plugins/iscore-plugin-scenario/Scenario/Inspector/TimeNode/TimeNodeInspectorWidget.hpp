#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <list>
#include <vector>
#include <Process/TimeValue.hpp>

namespace Inspector
{
class InspectorSectionWidget;
}
class QLabel;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Scenario
{
class MetadataWidget;
class TriggerInspectorWidget;
class EventModel;
class TimeNodeModel;
/*!
 * \brief The TimeNodeInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an TimeNode (Timebox) element.
 */
class TimeNodeInspectorWidget final : public Inspector::InspectorWidgetBase
{
    public:
        explicit TimeNodeInspectorWidget(
                const TimeNodeModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

        void addEvent(const EventModel& event);
        void removeEvent(const EventModel& event);

    private:
        QString tabName() override;

        void updateDisplayedValues();
        void on_splitTimeNodeClicked();
        void on_dateChanged(const TimeValue&);

        std::list<QWidget*> m_properties;
        QWidget* m_events;

        const TimeNodeModel& m_model;

        std::map<const Id<EventModel>, Inspector::InspectorSectionWidget*> m_eventList {};
        QLabel* m_date {};

        MetadataWidget* m_metadata {};

        TriggerInspectorWidget* m_trigwidg{};

};
}
