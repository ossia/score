// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "VerticalExtent.hpp"

#include <QPoint>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONValueVisitor.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Temporal/TemporalIntervalPresenter.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <Scenario/Document/TimeSync/TimeSyncPresenter.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockPresenter.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Process.hpp>

template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Process::ProcessModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Process::Cable>;

template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::IntervalModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::EventModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::TimeSyncModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::StateModel>;
template class SCORE_PLUGIN_SCENARIO_EXPORT score::EntityMap<Scenario::CommentBlockModel>;

template class SCORE_PLUGIN_SCENARIO_EXPORT IdContainer<Scenario::StatePresenter, Scenario::StateModel, void>;
template class SCORE_PLUGIN_SCENARIO_EXPORT IdContainer<Scenario::EventPresenter, Scenario::EventModel, void>;
template class SCORE_PLUGIN_SCENARIO_EXPORT IdContainer<Scenario::TimeSyncPresenter, Scenario::TimeSyncModel, void>;
template class SCORE_PLUGIN_SCENARIO_EXPORT IdContainer<Scenario::TemporalIntervalPresenter, Scenario::IntervalModel, void>;
template class SCORE_PLUGIN_SCENARIO_EXPORT IdContainer<Scenario::CommentBlockPresenter, Scenario::CommentBlockModel, void>;
template <>
void DataStreamReader::read(const Scenario::VerticalExtent& ve)
{
  m_stream << static_cast<QPointF>(ve);
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Scenario::VerticalExtent& ve)
{
  m_stream >> static_cast<QPointF&>(ve);
  checkDelimiter();
}

template <>
void JSONValueReader::read(const Scenario::VerticalExtent& ve)
{
  read(static_cast<QPointF>(ve));
}

template <>
void JSONValueWriter::write(Scenario::VerticalExtent& ve)
{
  write(static_cast<QPointF&>(ve));
}
