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


namespace Process
{
class LayerModel;
}
template <typename T>
class Reader;
template <typename T>
class Writer;
template <typename model>
class IdentifiedObject;


template <>
void DataStreamReader::read(const Scenario::SlotModel& slot)
{
  m_stream << slot.m_frontLayerModelId;

  const auto& lms = slot.layers;
  m_stream << (int32_t)lms.size();

  for (const Process::LayerModel& lm : lms)
  {
    // To allow recreation.
    // This supposes that the process is stored inside a Constraint.
    // We save the relative path coming from the layer's parent, since when
    // recreating the layer does not exist yet.
    m_stream << iscore::RelativePath(*lm.parent(), lm.processModel());

    readFrom(lm);
  }

  m_stream << slot.getHeight();

  insertDelimiter();
}


template <>
void DataStreamWriter::writeTo(Scenario::SlotModel& slot)
{
  OptionalId<Process::LayerModel> editedProcessId;
  m_stream >> editedProcessId;

  int32_t lm_size;
  m_stream >> lm_size;

  auto& layers = components.interfaces<Process::LayerFactoryList>();
  for (int i = 0; i < lm_size; i++)
  {
    iscore::RelativePath process;
    m_stream >> process;
    auto lm = deserialize_interface(layers, *this, process, &slot);
    if (lm)
      slot.layers.add(lm);
    else
      ISCORE_TODO;
  }

  qreal height;
  m_stream >> height;
  slot.setHeight(height);

  slot.putToFront(editedProcessId);

  checkDelimiter();
}


template <>
void JSONObjectReader::readFrom(const Scenario::SlotModel& slot)
{
  readFrom(static_cast<const iscore::Entity<Scenario::SlotModel>&>(slot));

  obj["EditedProcess"] = toJsonValue(slot.m_frontLayerModelId);
  obj["Height"] = slot.getHeight();

  // TODO toJsonArray
  QJsonArray arr;

  for (const Process::LayerModel& lm : slot.layers)
  {
    // See above.
    auto obj = toJsonObject(lm);
    obj["SharedProcess"] =
        toJsonObject(
          iscore::RelativePath(*lm.parent(), lm.processModel()));

    arr.push_back(std::move(obj));
  }

  obj["LayerModels"] = arr;
}


template <>
void JSONObjectWriter::writeTo(Scenario::SlotModel& slot)
{
  QJsonArray arr = obj["LayerModels"].toArray();

  auto& layers = components.interfaces<Process::LayerFactoryList>();
  for (const auto& json_vref : arr)
  {
    JSONObject::Deserializer des{json_vref.toObject()};
    auto process
        = fromJsonObject<iscore::RelativePath>(des.obj["SharedProcess"]);

    auto lm = deserialize_interface(layers, des, process, &slot);
    if (lm)
      slot.layers.add(lm);
    else
      ISCORE_TODO;
  }

  slot.setHeight(static_cast<qreal>(obj["Height"].toDouble()));
  auto editedProc
      = fromJsonValue<OptionalId<Process::LayerModel>>(obj["EditedProcess"]);
  slot.putToFront(editedProc);
}
