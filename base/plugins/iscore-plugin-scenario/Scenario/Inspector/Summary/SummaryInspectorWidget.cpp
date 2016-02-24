#include "SummaryInspectorWidget.hpp"

#include <QLabel>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>

namespace Scenario
{
SummaryInspectorWidget::SummaryInspectorWidget(const IdentifiedObjectAbstract* obj,
        std::set<const ConstraintModel*> constraints,
        std::set<const TimeNodeModel*> timenodes,
        std::set<const EventModel*> events,
        std::set<const StateModel*> states,
        const iscore::DocumentContext& context,
        QWidget* parent):
    InspectorWidgetBase {*obj, context, parent}
{
    setObjectName("SummaryInspectorWidget");
    setParent(parent);

    m_properties.push_back(new QLabel{tr("Constraints")});

    for(auto c : constraints)
    {
        m_properties.push_back(new QLabel{c->metadata.name()});
    }

    m_properties.push_back(new QLabel{tr("TimeNodes")});
    for(auto t : timenodes)
    {
        m_properties.push_back(new QLabel{t->metadata.name()});
    }

    updateAreaLayout(m_properties);
}

QString SummaryInspectorWidget::tabName()
{
    return tr("Summary");
}
}
