#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>

#include <QJsonObject>
#include <QJsonValue>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/path/RelativePath.hpp>
#include <iscore/model/Identifier.hpp>

template <typename T>
class Reader;
template <typename model>
class IdentifiedObject;

template <>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Reader<DataStream>>::readFrom_impl(
    const Process::LayerModel& layerModel)
{
  // To allow recreation.
  // This supposes that the process is stored inside a Constraint.
  // We save the relative path coming from the layer's parent, since when
  // recreating the layer does not exist yet.
  m_stream << iscore::RelativePath(
      *layerModel.parent(), layerModel.processModel());

  readFrom(
      static_cast<const IdentifiedObject<Process::LayerModel>&>(layerModel));

  // LayerModel doesn't have any particular data to save
}

template <>
ISCORE_LIB_PROCESS_EXPORT void Visitor<Reader<JSONObject>>::readFrom_impl(
    const Process::LayerModel& layerModel)
{
  // See above.
  m_obj["SharedProcess"] = toJsonObject(
      iscore::RelativePath(*layerModel.parent(), layerModel.processModel()));

  readFrom(
      static_cast<const IdentifiedObject<Process::LayerModel>&>(layerModel));

  // LayerModel doesn't have any particular data to save
}
