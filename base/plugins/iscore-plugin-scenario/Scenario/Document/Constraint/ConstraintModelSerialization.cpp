#include <Process/Process.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/FullView/FullViewConstraintViewModel.hpp>

#include <iscore/tools/std/Optional.hpp>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <algorithm>
#include <sys/types.h>

#include <Process/ProcessList.hpp>
#include <Process/TimeValue.hpp>
#include <iscore/application/ApplicationContext.hpp>
#include <iscore/model/ModelMetadata.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/Identifier.hpp>

namespace Scenario
{
class StateModel;
}

template <typename T>
class IdentifiedObject;
template <typename T>
class Reader;
template <typename T>
class Writer;

// Note : comment gérer le cas d'un process shared model qui ne sait se
// sérializer qu'en binaire, dans du json?
// Faire passer l'info en base64 ?

template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void DataStreamReader::read(
    const Scenario::ConstraintModel& constraint)
{
  // Processes
  m_stream << (int32_t)constraint.processes.size();
  for (const auto& process : constraint.processes)
  {
    readFrom(process);
  }

  // Rackes
  m_stream << constraint.smallViewRack()
           << constraint.fullViewRack();

  // Common data
  m_stream << constraint.duration << constraint.m_startState
           << constraint.m_endState

           << constraint.m_startDate << constraint.m_heightPercentage
           << constraint.m_zoom << constraint.m_center
           << constraint.m_smallViewShown;

  insertDelimiter();
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
DataStreamWriter::write(Scenario::ConstraintModel& constraint)
{
  // Processes
  int32_t process_count;
  m_stream >> process_count;

  auto& pl = components.interfaces<Process::ProcessFactoryList>();
  for (; process_count-- > 0;)
  {
    auto proc = deserialize_interface(pl, *this, &constraint);
    if (proc)
    {
      // TODO why isn't AddProcess used here ?!
      constraint.processes.add(proc);
    }
    else
    {
      ISCORE_TODO;
    }
  }

  // Rackes
  constraint.m_smallViewRack = new Scenario::RackModel(*this, &constraint);
  constraint.m_fullViewRack = new Scenario::RackModel(*this, &constraint);

  // Common data
  m_stream >> constraint.duration >> constraint.m_startState
      >> constraint.m_endState

      >> constraint.m_startDate >> constraint.m_heightPercentage
      >> constraint.m_zoom >> constraint.m_center
      >> constraint.m_smallViewShown;

  checkDelimiter();
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void JSONObjectReader::read(
    const Scenario::ConstraintModel& constraint)
{
  // Processes
  obj[strings.Processes] = toJsonArray(constraint.processes);

  // Rackes
  obj[strings.SmallViewRack] = toJsonObject(constraint.smallViewRack());
  obj[strings.FullViewRack] = toJsonObject(constraint.fullViewRack());

  // Common data
  // The fields will go in the same level as the
  // rest of the constraint
  readFrom(constraint.duration);

  obj[strings.StartState] = toJsonValue(constraint.m_startState);
  obj[strings.EndState] = toJsonValue(constraint.m_endState);

  obj[strings.StartDate] = toJsonValue(constraint.m_startDate);
  obj[strings.HeightPercentage] = constraint.m_heightPercentage;

  obj[strings.Zoom] = constraint.m_zoom;
  obj[strings.Center] = toJsonValue(constraint.m_center);
  obj[strings.SmallViewShown] = constraint.m_smallViewShown;
}


template <>
ISCORE_PLUGIN_SCENARIO_EXPORT void
JSONObjectWriter::write(Scenario::ConstraintModel& constraint)
{
  auto& pl = components.interfaces<Process::ProcessFactoryList>();

  QJsonArray process_array = obj[strings.Processes].toArray();
  for (const auto& json_vref : process_array)
  {
    JSONObject::Deserializer deserializer{json_vref.toObject()};
    auto proc = deserialize_interface(pl, deserializer, &constraint);
    if (proc)
      constraint.processes.add(proc);
    else
      ISCORE_TODO;
  }

  {
    JSONObject::Deserializer dr{obj[strings.SmallViewRack].toObject()};
    constraint.m_smallViewRack = new Scenario::RackModel(dr, &constraint);
  }

  {
    JSONObject::Deserializer dr{obj[strings.FullViewRack].toObject()};
    constraint.m_fullViewRack = new Scenario::RackModel(dr, &constraint);
  }

  writeTo(constraint.duration);
  constraint.m_startState
      = fromJsonValue<Id<Scenario::StateModel>>(obj[strings.StartState]);
  constraint.m_endState
      = fromJsonValue<Id<Scenario::StateModel>>(obj[strings.EndState]);

  constraint.m_startDate = fromJsonValue<TimeVal>(obj[strings.StartDate]);
  constraint.m_heightPercentage = obj[strings.HeightPercentage].toDouble();

  constraint.m_zoom = obj[strings.Zoom].toDouble();
  constraint.m_center = fromJsonValue<QRectF>(obj[strings.Center]);
  constraint.m_smallViewShown = obj[strings.SmallViewShown].toBool();
}
