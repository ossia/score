#pragma once

/*
This file is used to define simple data structure to simplify the code when
needed
*/

#include <Process/TimeValue.hpp>
#include <QByteArray>
#include <QMap>
#include <QPair>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <iscore/model/path/Path.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/tools/std/HashMap.hpp>
#include <iscore_plugin_scenario_export.h>

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

struct ISCORE_PLUGIN_SCENARIO_EXPORT IntervalSaveData
{
  IntervalSaveData() = default;
  IntervalSaveData(const IntervalModel&);

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
  iscore::hash_map<Id<TimeSyncModel>, TimenodeProperties> timesyncs;
  iscore::hash_map<Id<IntervalModel>, IntervalProperties> intervals;
};
}
