// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SummaryInspectorWidget.hpp"

#include <QLabel>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Inspector/Constraint/ConstraintSummaryWidget.hpp>
#include <Scenario/Inspector/Event/EventSummaryWidget.hpp>
#include <Scenario/Inspector/TimeNode/TimeNodeSummaryWidget.hpp>

namespace Scenario
{
SummaryInspectorWidget::SummaryInspectorWidget(
    const IdentifiedObjectAbstract* obj,
    const std::set<const ConstraintModel*>&
        constraints,
    const std::set<const TimeNodeModel*>&
        timenodes,
    const std::set<const EventModel*>&
        events,
    const std::set<const StateModel*>&
        states,
    const iscore::DocumentContext& context,
    QWidget* parent)
    : InspectorWidgetBase{*obj, context, parent}
{
  setObjectName("SummaryInspectorWidget");
  setParent(parent);

  auto cSection
      = new Inspector::InspectorSectionWidget{tr("Constraints"), false, this};
  m_properties.push_back(cSection);

  for (auto c : constraints)
  {
    cSection->addContent(new ConstraintSummaryWidget{*c, context, this});
  }

  auto tnSection
      = new Inspector::InspectorSectionWidget{tr("TimeNodes"), false, this};
  m_properties.push_back(tnSection);
  for (auto t : timenodes)
  {
    tnSection->addContent(new TimeNodeSummaryWidget{*t, context, this});
  }

  auto evSection
      = new Inspector::InspectorSectionWidget{tr("Events"), false, this};
  m_properties.push_back(evSection);
  for (auto ev : events)
  {
    evSection->addContent(new EventSummaryWidget{*ev, context, this});
  }

  updateAreaLayout(m_properties);
}

QString SummaryInspectorWidget::tabName()
{
  return tr("Summary");
}
}
