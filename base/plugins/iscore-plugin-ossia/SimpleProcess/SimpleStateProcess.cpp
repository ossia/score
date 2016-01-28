#include "SimpleStateProcess.hpp"


template<>
void Visitor<Reader<DataStream>>::readFrom(const SimpleStateProcessModel& proc)
{

    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(SimpleStateProcessModel& proc)
{
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const SimpleStateProcessModel& proc)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(SimpleStateProcessModel& proc)
{
}

