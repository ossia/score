#include <ProcessInterface/TimeValue.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>


template<>
void Visitor<Reader<DataStream>>::readFrom(const TimeValue& tv)
{
    m_stream << tv.isInfinite();

    if(!tv.isInfinite())
    {
        m_stream << tv.msec();
    }

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(TimeValue& tv)
{
    bool inf;
    double msec;
    m_stream >> inf;

    if(!inf)
    {
        m_stream >> msec;
        tv.setMSecs(msec);
    }
    else
    {
        tv = TimeValue {PositiveInfinity{}};
    }

    checkDelimiter();
}

/*
template<>
void Visitor<Reader<JSONObject>>::readFrom(const TimeValue& tv)
{
    if(tv.isInfinite())
    {
        m_obj["Time"] = "inf";
    }
    else
    {
        m_obj["Time"] = tv.msec();
    }
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(TimeValue& tv)
{
    if(m_obj["Time"].toString() == "inf")
    {
        tv = TimeValue {PositiveInfinity{}};
    }
    else
    {
        tv.setMSecs(m_obj["Time"].toDouble());
    }
}
*/

template<>
void Visitor<Reader<JSONValue>>::readFrom(const TimeValue& tv)
{
    if(tv.isInfinite())
    {
        val = "inf";
    }
    else
    {
        val = tv.msec();
    }
}

template<>
void Visitor<Writer<JSONValue>>::writeTo(TimeValue& tv)
{
    if(val.toString() == "inf")
    {
        tv = TimeValue {PositiveInfinity{}};
    }
    else
    {
        tv.setMSecs(val.toDouble());
    }
}
