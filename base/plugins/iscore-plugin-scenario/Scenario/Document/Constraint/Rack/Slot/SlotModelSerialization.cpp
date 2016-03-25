#include <Scenario/Document/Constraint/LayerModelLoader.hpp>

#include <boost/optional/optional.hpp>
#include <QtGlobal>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <sys/types.h>
#include <algorithm>

#include "SlotModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

namespace Process { class LayerModel; }
template <typename T> class Reader;
template <typename T> class Writer;
template <typename model> class IdentifiedObject;

template<> void Visitor<Reader<DataStream>>::readFrom(const Scenario::SlotModel& slot)
{
    readFrom(static_cast<const IdentifiedObject<Scenario::SlotModel>&>(slot));
    readFrom(slot.metadata);

    m_stream << slot.m_frontLayerModelId;

    const auto& lms = slot.layers;
    m_stream << (int32_t) lms.size();

    for(const auto& lm : lms)
    {
        readFrom(lm);
    }

    m_stream << slot.height();

    insertDelimiter();
}

template<> void Visitor<Writer<DataStream>>::writeTo(Scenario::SlotModel& slot)
{
    writeTo(slot.metadata);

    Id<Process::LayerModel> editedProcessId;
    m_stream >> editedProcessId;

    int32_t lm_size;
    m_stream >> lm_size;

    const auto& cstr = slot.parentConstraint();

    for(int i = 0; i < lm_size; i++)
    {
        auto lm = Process::createLayerModel(*this, cstr, &slot);
        slot.layers.add(lm);
    }

    qreal height;
    m_stream >> height;
    slot.setHeight(height);

    slot.putToFront(editedProcessId);

    checkDelimiter();
}





template<> void Visitor<Reader<JSONObject>>::readFrom(const Scenario::SlotModel& slot)
{
    readFrom(static_cast<const IdentifiedObject<Scenario::SlotModel>&>(slot));
    m_obj["Metadata"] = toJsonObject(slot.metadata);

    m_obj["EditedProcess"] = toJsonValue(slot.m_frontLayerModelId);
    m_obj["Height"] = slot.height();

    QJsonArray arr;

    for(const auto& lm : slot.layers)
    {
        arr.push_back(toJsonObject(lm));
    }

    m_obj["LayerModels"] = arr;
}

template<> void Visitor<Writer<JSONObject>>::writeTo(Scenario::SlotModel& slot)
{
    slot.metadata = fromJsonObject<ModelMetadata>(m_obj["Metadata"]);
    QJsonArray arr = m_obj["LayerModels"].toArray();

    const auto& cstr = slot.parentConstraint();

    for(const auto& json_vref : arr)
    {
        Deserializer<JSONObject> deserializer {json_vref.toObject() };
        auto lm = Process::createLayerModel(deserializer,
                                          cstr,
                                          &slot);
        slot.layers.add(lm);
    }

    slot.setHeight(static_cast<qreal>(m_obj["Height"].toDouble()));
    slot.putToFront(
                fromJsonValue<Id<Process::LayerModel>>(
                    m_obj["EditedProcess"]));
}
