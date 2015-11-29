#include <Process/TimeValue.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <qjsonvalue.h>
#include <qstring.h>

#include "iscore/serialization/JSONValueVisitor.hpp"

template <typename T> class Reader;
template <typename T> class Writer;


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
    m_stream >> inf;

    if(!inf)
    {
        double msec;
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
