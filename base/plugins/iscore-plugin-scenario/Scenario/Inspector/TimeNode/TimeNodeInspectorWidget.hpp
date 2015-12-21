#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <list>
#include <vector>

class EventShortCut;
class InspectorSectionWidget;
class MetadataWidget;
class QLabel;
class QWidget;
class TimeNodeModel;
class TriggerInspectorWidget;
namespace iscore {
class Document;
}  // namespace iscore

/*!
 * \brief The TimeNodeInspectorWidget class
 *      Inherits from InspectorWidgetInterface. Manages an inteface for an TimeNode (Timebox) element.
 */
class TimeNodeInspectorWidget final : public InspectorWidgetBase
{
    public:
        explicit TimeNodeInspectorWidget(
                const TimeNodeModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    private:
        void updateDisplayedValues();
        void on_splitTimeNodeClicked();

        std::list<QWidget*> m_properties;
        std::vector<EventShortCut*> m_events;

        const TimeNodeModel& m_model;

        InspectorSectionWidget* m_eventList {};
        QLabel* m_date {};

        MetadataWidget* m_metadata {};

        TriggerInspectorWidget* m_trigwidg{};

};
