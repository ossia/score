#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <list>
#include <vector>

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
class EventShortCut;
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

    private:
        QString tabName() override;

        void updateDisplayedValues();
        void on_splitTimeNodeClicked();

        std::list<QWidget*> m_properties;
        std::vector<EventShortCut*> m_events;

        const TimeNodeModel& m_model;

        Inspector::InspectorSectionWidget* m_eventList {};
        QLabel* m_date {};

        MetadataWidget* m_metadata {};

        TriggerInspectorWidget* m_trigwidg{};

};
}
