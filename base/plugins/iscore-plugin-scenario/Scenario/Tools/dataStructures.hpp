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
class ConstraintModel;
class TimeNodeModel;

struct TimenodeProperties
{
  TimeVal oldDate;
  TimeVal newDate;
  double date;

  double date_min;
  double date_max;

  ExecutionStatus status{ExecutionStatus::Editing};
};

struct ISCORE_PLUGIN_SCENARIO_EXPORT ConstraintSaveData
{
  ConstraintSaveData() = default;
  ConstraintSaveData(const ConstraintModel&);

  void reload(ConstraintModel&) const;

  Path<ConstraintModel> constraintPath;
  QVector<QByteArray> processes;
  QVector<QByteArray> racks;
};

struct ConstraintProperties : public ConstraintSaveData
{
  using ConstraintSaveData::ConstraintSaveData;

  TimeVal oldMin;
  TimeVal newMin;
  TimeVal oldMax;
  TimeVal newMax;
  ExecutionStatus status{ExecutionStatus::Editing};
};

struct ElementsProperties
{
  iscore::hash_map<Id<TimeNodeModel>, TimenodeProperties> timenodes;
  iscore::hash_map<Id<ConstraintModel>, ConstraintProperties> constraints;
};
}
