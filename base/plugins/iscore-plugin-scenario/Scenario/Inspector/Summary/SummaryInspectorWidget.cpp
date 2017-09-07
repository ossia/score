// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SummaryInspectorWidget.hpp"

#include <QLabel>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>

#include <Inspector/InspectorSectionWidget.hpp>
#include <Scenario/Inspector/Interval/IntervalSummaryWidget.hpp>
#include <Scenario/Inspector/Event/EventSummaryWidget.hpp>
#include <Scenario/Inspector/TimeSync/TimeSyncSummaryWidget.hpp>

namespace Scenario
{
SummaryInspectorWidget::SummaryInspectorWidget(
    const IdentifiedObjectAbstract* obj,
    const std::set<const IntervalModel*>&
        intervals,
    const std::set<const TimeSyncModel*>&
        timesyncs,
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
      = new Inspector::InspectorSectionWidget{tr("Intervals"), false, this};
  m_properties.push_back(cSection);

  for (auto c : intervals)
  {
    cSection->addContent(new IntervalSummaryWidget{*c, context, this});
  }

  auto tnSection
      = new Inspector::InspectorSectionWidget{tr("Syncs"), false, this};
  m_properties.push_back(tnSection);
  for (auto t : timesyncs)
  {
    tnSection->addContent(new TimeSyncSummaryWidget{*t, context, this});
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
