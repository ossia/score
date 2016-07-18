#pragma once
#include <Inspector/InspectorWidgetBase.hpp>
#include <list>
#include <vector>
#include <set>

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
                std::vector<const ConstraintModel*> constraints,
                std::vector<const TimeNodeModel*> timenodes,
                std::vector<const EventModel*> events,
                std::vector<const StateModel*> states,
                const iscore::DocumentContext& context,
                QWidget* parent = nullptr);

        QString tabName() override;

    private:

        std::list<QWidget*> m_properties;

};

}
