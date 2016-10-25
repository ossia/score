#include <Scenario/Document/Constraint/LayerModelLoader.hpp>

#include <iscore/tools/std/Optional.hpp>
#include <QtGlobal>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <sys/types.h>
#include <algorithm>

#include <Process/ProcessList.hpp>
#include "SlotModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/EntityMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Process { class LayerModel; }
template <typename T> class Reader;
template <typename T> class Writer;
template <typename model> class IdentifiedObject;

template<> void Visitor<Reader<DataStream>>::readFrom(const Scenario::SlotModel& slot)
{
    readFrom(static_cast<const iscore::Entity<Scenario::SlotModel>&>(slot));

    m_stream << slot.m_frontLayerModelId;

    const auto& lms = slot.layers;
    m_stream << (int32_t) lms.size();

    for(const auto& lm : lms)
    {
        readFrom(lm);
    }

    m_stream << slot.getHeight();

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(Scenario::SlotModel& slot)
{
    OptionalId<Process::LayerModel> editedProcessId;
    m_stream >> editedProcessId;

    int32_t lm_size;
    m_stream >> lm_size;

    auto& layers = components.factory<Process::LayerFactoryList>();
    for(int i = 0; i < lm_size; i++)
    {
        auto lm = deserialize_interface(layers, *this, &slot);
        if(lm)
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





template<> void Visitor<Reader<JSONObject>>::readFrom(const Scenario::SlotModel& slot)
{
    readFrom(static_cast<const iscore::Entity<Scenario::SlotModel>&>(slot));

    m_obj["EditedProcess"] = toJsonValue(slot.m_frontLayerModelId);
    m_obj["Height"] = slot.getHeight();

    // TODO toJsonArray
    QJsonArray arr;

    for(const auto& lm : slot.layers)
    {
        arr.push_back(toJsonObject(lm));
    }

    m_obj["LayerModels"] = arr;
}

template<> void Visitor<Writer<JSONObject>>::writeTo(Scenario::SlotModel& slot)
{
    QJsonArray arr = m_obj["LayerModels"].toArray();

    auto& layers = components.factory<Process::LayerFactoryList>();
    for(const auto& json_vref : arr)
    {
        Deserializer<JSONObject> deserializer {json_vref.toObject() };
        auto lm = deserialize_interface(layers, deserializer, &slot);
        if(lm)
            slot.layers.add(lm);
        else
            ISCORE_TODO;
    }

    slot.setHeight(static_cast<qreal>(m_obj["Height"].toDouble()));
    auto editedProc = fromJsonValue<OptionalId<Process::LayerModel>>(m_obj["EditedProcess"]);
    slot.putToFront(editedProc);
}
