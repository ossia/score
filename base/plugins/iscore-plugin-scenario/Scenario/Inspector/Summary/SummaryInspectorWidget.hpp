#pragma once
#include <Inspector/InspectorWidgetBase.hpp>
#include <list>
#include <vector>


namespace Scenario
{
class ConstraintModel;
class TimeNodeModel;
class EventModel;
class StateModel;
class SummaryInspectorWidget final : public Inspector::InspectorWidgetBase
{
    public:
        SummaryInspectorWidget(
                const IdentifiedObjectAbstract* obj,
                std::set<const ConstraintModel*> constraints,
                std::set<const TimeNodeModel*> timenodes,
                std::set<const EventModel*> events,
                std::set<const StateModel*> states,
                const iscore::DocumentContext& context,
                QWidget* parent = 0);

        QString tabName() override;

    private:

        std::list<QWidget*> m_properties;

};

}
