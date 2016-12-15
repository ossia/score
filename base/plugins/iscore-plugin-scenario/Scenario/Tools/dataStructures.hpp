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
#include <iscore_plugin_scenario_export.h>

namespace Scenario
{
class ConstraintModel;
class ConstraintViewModel;
class RackModel;
class TimeNodeModel;

struct TimenodeProperties
{
  TimeValue oldDate;
  TimeValue newDate;
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
  QMap< // Mapping for the view models of this constraint
      Id<ConstraintViewModel>, OptionalId<RackModel>>
      viewMapping;
};

struct ConstraintProperties : public ConstraintSaveData
{
  using ConstraintSaveData::ConstraintSaveData;

  TimeValue oldMin;
  TimeValue newMin;
  TimeValue oldMax;
  TimeValue newMax;
  ExecutionStatus status{ExecutionStatus::Editing};
};

struct ElementsProperties
{
  QMap<Id<TimeNodeModel>, TimenodeProperties> timenodes;
  QMap<Id<ConstraintModel>, ConstraintProperties> constraints;
};
}
