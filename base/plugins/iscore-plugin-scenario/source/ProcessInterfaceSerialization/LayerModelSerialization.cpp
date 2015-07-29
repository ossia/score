#include "LayerModelSerialization.hpp"
#include "ProcessInterface/ProcessModel.hpp"
#include "ProcessInterface/LayerModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"

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
LayerModel* createLayerModel(Deserializer<DataStream>& deserializer,
        const ConstraintModel& constraint,
        QObject* parent)
{
    id_type<Process> sharedProcessId;
    deserializer.m_stream >> sharedProcessId;

    auto& process = constraint.process(sharedProcessId);
    auto viewmodel = process.loadLayer(deserializer.toVariant(),
                                       parent);

    deserializer.checkDelimiter();

    return viewmodel;
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

template<>
LayerModel* createLayerModel(
        Deserializer<JSONObject>& deserializer,
        const ConstraintModel& constraint,
        QObject* parent)
{
    auto& process = constraint.process(
                fromJsonValue<id_type<Process>>(deserializer.m_obj["SharedProcessId"]));
    auto viewmodel = process.loadLayer(deserializer.toVariant(),
                                            parent);

    return viewmodel;
}
