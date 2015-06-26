#include "SlotModel.hpp"
#include "ProcessInterface/ProcessViewModel.hpp"
#include "source/ProcessInterfaceSerialization/ProcessViewModelSerialization.hpp"
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>

template<> void Visitor<Reader<DataStream>>::readFrom(const SlotModel& slot)
{
    readFrom(static_cast<const IdentifiedObject<SlotModel>&>(slot));

    m_stream << slot.frontProcessViewModel();

    auto pvms = slot.processViewModels();
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
    id_type<ProcessViewModel> editedProcessId;
    m_stream >> editedProcessId;

    int pvm_size;
    m_stream >> pvm_size;

    const auto& cstr = slot.parentConstraint();

    for(int i = 0; i < pvm_size; i++)
    {
        auto pvm = createProcessViewModel(*this, cstr, &slot);
        slot.addProcessViewModel(pvm);
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

    m_obj["EditedProcess"] = toJsonValue(slot.frontProcessViewModel());
    m_obj["Height"] = slot.height();

    QJsonArray arr;

    for(const auto& pvm : slot.processViewModels())
    {
        arr.push_back(toJsonObject(*pvm));
    }

    m_obj["ProcessViewModels"] = arr;
}

template<> void Visitor<Writer<JSONObject>>::writeTo(SlotModel& slot)
{
    QJsonArray arr = m_obj["ProcessViewModels"].toArray();

    const auto& cstr = slot.parentConstraint();

    for(const auto& json_vref : arr)
    {
        Deserializer<JSONObject> deserializer {json_vref.toObject() };
        auto pvm = createProcessViewModel(deserializer,
                                          cstr,
                                          &slot);
        slot.addProcessViewModel(pvm);
    }

    slot.setHeight(m_obj["Height"].toInt());
    slot.putToFront(
                fromJsonValue<id_type<ProcessViewModel>>(
                    m_obj["EditedProcess"]));
}
