#include <Process/Process.hpp>
#include <Process/LayerModel.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const LayerModel& layerModel)
{
    // To allow recration using createLayerModel.
    // This supposes that the process is stored inside a Constraint.
    m_stream << layerModel.processModel().id();

    readFrom(static_cast<const IdentifiedObject<LayerModel>&>(layerModel));

    // LayerModel doesn't have any particular data to save

    // Save the subclass
    layerModel.serialize(toVariant());

    insertDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const LayerModel& layerModel)
{
    // To allow recration using createLayerModel.
    // This supposes that the process is stored inside a Constraint.
    m_obj["SharedProcessId"] = toJsonValue(layerModel.processModel().id());

    readFrom(static_cast<const IdentifiedObject<LayerModel>&>(layerModel));

    // LayerModel doesn't have any particular data to save

    // Save the subclass
    layerModel.serialize(toVariant());
}
