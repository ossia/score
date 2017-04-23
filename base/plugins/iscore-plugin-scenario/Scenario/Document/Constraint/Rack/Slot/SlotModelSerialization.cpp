#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QtGlobal>
#include <algorithm>
#include <iscore/tools/std/Optional.hpp>
#include <sys/types.h>

#include "SlotModel.hpp"
#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/EntityMap.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/model/path/RelativePath.hpp>

template <>
void DataStreamReader::read(const Scenario::SlotModel& slot)
{
  m_stream << slot.m_frontLayerModelId << slot.m_processes << slot.getHeight();

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Scenario::SlotModel& slot)
{
  OptionalId<Process::ProcessModel> editedProcessId;
  m_stream >> editedProcessId;

  std::vector<Id<Process::ProcessModel>> procs;
  m_stream >> procs;

  qreal height;
  m_stream >> height;

  slot.m_processes = std::move(procs);
  slot.setHeight(height);

  slot.putToFront(editedProcessId);

  checkDelimiter();
}


template <>
void JSONObjectReader::read(const Scenario::SlotModel& slot)
{
  obj["EditedProcess"] = toJsonValue(slot.m_frontLayerModelId);
  obj["Height"] = slot.getHeight();
  obj["Processes"] = toJsonValueArray(slot.m_processes);
}


template <>
void JSONObjectWriter::write(Scenario::SlotModel& slot)
{
  QJsonArray arr = obj["LayerModels"].toArray();
  slot.m_processes.clear();
  for(const auto& e : arr)
  {
    slot.m_processes.push_back(fromJsonValue<Id<Process::ProcessModel>>(e));
  }
  slot.setHeight(static_cast<qreal>(obj["Height"].toDouble()));
  auto editedProc
      = fromJsonValue<OptionalId<Process::ProcessModel>>(obj["EditedProcess"]);
  slot.putToFront(editedProc);
}
