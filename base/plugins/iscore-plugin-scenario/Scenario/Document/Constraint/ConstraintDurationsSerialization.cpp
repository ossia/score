#include <QJsonObject>
#include <QJsonValue>

#include "ConstraintDurations.hpp"
#include <Process/TimeValue.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T> class Reader;
template <typename T> class Writer;

template<> void Visitor<Reader<DataStream>>::readFrom(const ConstraintDurations& durs)
{
    m_stream
            << durs.m_defaultDuration
            << durs.m_minDuration
            << durs.m_maxDuration
            << durs.m_rigidity;
}

template<> void Visitor<Writer<DataStream>>::writeTo(ConstraintDurations& durs)
{
    m_stream
            >> durs.m_defaultDuration
            >> durs.m_minDuration
            >> durs.m_maxDuration
            >> durs.m_rigidity;
}

template<> void Visitor<Reader<JSONObject>>::readFrom(const ConstraintDurations& durs)
{
    m_obj["DefaultDuration"] = toJsonValue(durs.m_defaultDuration);
    m_obj["MinDuration"] = toJsonValue(durs.m_minDuration);
    m_obj["MaxDuration"] = toJsonValue(durs.m_maxDuration);
    m_obj["Rigidity"] = durs.m_rigidity;
}

template<> void Visitor<Writer<JSONObject>>::writeTo(ConstraintDurations& durs)
{
    durs.m_defaultDuration = fromJsonValue<TimeValue> (m_obj["DefaultDuration"]);
    durs.m_minDuration = fromJsonValue<TimeValue> (m_obj["MinDuration"]);
    durs.m_maxDuration = fromJsonValue<TimeValue> (m_obj["MaxDuration"]);
    durs.m_rigidity = m_obj["Rigidity"].toBool();
}
