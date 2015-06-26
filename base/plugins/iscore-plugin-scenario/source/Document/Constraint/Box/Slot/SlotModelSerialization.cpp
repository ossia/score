#include "SlotModel.hpp"
#include "ProcessInterface/LayerModel.hpp"
#include "source/ProcessInterfaceSerialization/LayerModelSerialization.hpp"
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>

template<> void Visitor<Reader<DataStream>>::readFrom(const SlotModel& slot)
{
    readFrom(static_cast<const IdentifiedObject<SlotModel>&>(slot));

    m_stream << slot.frontLayerModel();

    auto pvms = slot.layerModels();
    m_stream << (int) pvms.size();

    for(auto pvm : pvms)
    {
        readFrom(*pvm);
    }

    m_stream << slot.height();

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(SlotModel& slot)
{
    id_type<LayerModel> editedProcessId;
    m_stream >> editedProcessId;

    int pvm_size;
    m_stream >> pvm_size;

    const auto& cstr = slot.parentConstraint();

    for(int i = 0; i < pvm_size; i++)
    {
        auto pvm = createLayerModel(*this, cstr, &slot);
        slot.addLayerModel(pvm);
    }

    int height;
    m_stream >> height;
    slot.setHeight(height);

    slot.putToFront(editedProcessId);

    checkDelimiter();
}





template<> void Visitor<Reader<JSONObject>>::readFrom(const SlotModel& slot)
{
    readFrom(static_cast<const IdentifiedObject<SlotModel>&>(slot));

    m_obj["EditedProcess"] = toJsonValue(slot.frontLayerModel());
    m_obj["Height"] = slot.height();

    QJsonArray arr;

    for(const auto& pvm : slot.layerModels())
    {
        arr.push_back(toJsonObject(*pvm));
    }

    m_obj["LayerModels"] = arr;
}

template<> void Visitor<Writer<JSONObject>>::writeTo(SlotModel& slot)
{
    QJsonArray arr = m_obj["LayerModels"].toArray();

    const auto& cstr = slot.parentConstraint();

    for(const auto& json_vref : arr)
    {
        Deserializer<JSONObject> deserializer {json_vref.toObject() };
        auto pvm = createLayerModel(deserializer,
                                          cstr,
                                          &slot);
        slot.addLayerModel(pvm);
    }

    slot.setHeight(m_obj["Height"].toInt());
    slot.putToFront(
                fromJsonValue<id_type<LayerModel>>(
                    m_obj["EditedProcess"]));
}
