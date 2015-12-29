#include <Process/LayerModel.hpp>
#include <Process/Process.hpp>

#include <QJsonObject>
#include <QJsonValue>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

template <typename T> class Reader;
template <typename model> class IdentifiedObject;

template<>
void Visitor<Reader<DataStream>>::readFrom(const Process::LayerModel& layerModel)
{
    // To allow recration using createLayerModel.
    // This supposes that the process is stored inside a Constraint.
    m_stream << layerModel.processModel().id();

    readFrom(static_cast<const IdentifiedObject<Process::LayerModel>&>(layerModel));

    // LayerModel doesn't have any particular data to save

    // Save the subclass
    layerModel.serialize(toVariant());

    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const Process::LayerModel& layerModel)
{
    // To allow recration using createLayerModel.
    // This supposes that the process is stored inside a Constraint.
    m_obj["SharedProcessId"] = toJsonValue(layerModel.processModel().id());

    readFrom(static_cast<const IdentifiedObject<Process::LayerModel>&>(layerModel));

    // LayerModel doesn't have any particular data to save

    // Save the subclass
    layerModel.serialize(toVariant());
}
