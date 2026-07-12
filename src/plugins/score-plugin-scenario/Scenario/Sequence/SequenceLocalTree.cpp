// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "SequenceLocalTree.hpp"

namespace LocalTree
{

SequenceComponentBase::SequenceComponentBase(
    ossia::net::node_base& parent, Sequence::SequenceModel& sequence,
    const score::DocumentContext& sys, QObject* parent_obj)
    : ProcessComponent_T<
        Sequence::SequenceModel>{parent, sequence, sys, "SequenceComponent", parent_obj}
    , m_intervalsNode{*node().create_child("sections")}
    , m_eventsNode{*node().create_child("events")}
    , m_timeSyncsNode{*node().create_child("syncs")}
    , m_statesNode{*node().create_child("states")}
{
}

template <>
Interval* SequenceComponentBase::make<Interval, Scenario::IntervalModel>(
    Scenario::IntervalModel& elt)
{
  return new Interval{m_intervalsNode, elt, system(), this};
}

template <>
Event*
SequenceComponentBase::make<Event, Scenario::EventModel>(Scenario::EventModel& elt)
{
  return new Event{m_eventsNode, elt, system(), this};
}

template <>
TimeSync* SequenceComponentBase::make<TimeSync, Scenario::TimeSyncModel>(
    Scenario::TimeSyncModel& elt)
{
  return new TimeSync{m_timeSyncsNode, elt, system(), this};
}

template <>
State*
SequenceComponentBase::make<State, Scenario::StateModel>(Scenario::StateModel& elt)
{
  return new State{m_statesNode, elt, system(), this};
}
}
