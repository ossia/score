#pragma once

/*
This file is used to define simple data structure to simplify the code when
needed
*/

#include <Dataflow/Commands/CableHelpers.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>

#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/HashMap.hpp>

#include <QByteArray>
#include <QMap>
#include <QPair>

#include <score_plugin_scenario_export.h>

namespace Scenario
{
class IntervalModel;
class TimeSyncModel;

struct TimenodeProperties
{
  TimeVal oldDate{};
  TimeVal newDate{};
  double date{};

  double date_min{};
  double date_max{};

  ExecutionStatus status{ExecutionStatus::Editing};
};

struct SCORE_PLUGIN_SCENARIO_EXPORT IntervalSaveData
{
  IntervalSaveData() = default;
  IntervalSaveData(const IntervalModel&, bool saveIntemporal);

  void reload(IntervalModel&) const;

  Path<IntervalModel> intervalPath;
  QVector<QByteArray> processes;
  QVector<QByteArray> racks;
};

struct IntervalProperties : public IntervalSaveData
{
  using IntervalSaveData::IntervalSaveData;

  TimeVal oldDefault{};
  TimeVal oldMin{};
  TimeVal newMin{};
  TimeVal oldMax{};
  TimeVal newMax{};
  ExecutionStatus status{ExecutionStatus::Editing};
};

struct ElementsProperties
{
  score::hash_map<Id<TimeSyncModel>, TimenodeProperties> timesyncs;
  score::hash_map<Id<IntervalModel>, IntervalProperties> intervals;

  Dataflow::SerializedCables cables;
};
}
