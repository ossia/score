#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>

#include <QJsonObject>
#include <QJsonValue>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/model/path/PathSerialization.hpp>
#include <iscore/model/Identifier.hpp>


template <>
ISCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(
    const Process::LayerModel& layerModel)
{
  // LayerModel doesn't have any particular data to save
}

template <>
ISCORE_LIB_PROCESS_EXPORT void JSONObjectReader::readFromConcrete(
    const Process::LayerModel& layerModel)
{
  readFrom(
      static_cast<const IdentifiedObject<Process::LayerModel>&>(layerModel));

  // LayerModel doesn't have any particular data to save
}
