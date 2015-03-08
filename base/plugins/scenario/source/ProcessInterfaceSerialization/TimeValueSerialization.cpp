#include <ProcessInterface/TimeValue.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>


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
    int msec;
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

template<>
void Visitor<Reader<JSON>>::readFrom(const TimeValue& tv)
{
    m_obj["Infinite"] = tv.isInfinite();

    if(!tv.isInfinite())
    {
        m_obj["Time"] = tv.msec();
    }
}

template<>
void Visitor<Writer<JSON>>::writeTo(TimeValue& tv)
{
    if(m_obj["Infinite"].toBool())
    {
        tv = TimeValue {PositiveInfinity{}};
    }
    else
    {
        tv.setMSecs(m_obj["Time"].toInt());
    }
}
