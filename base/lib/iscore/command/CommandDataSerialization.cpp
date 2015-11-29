#include "CommandData.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>

template<>
void Visitor<Reader<DataStream>>::readFrom(const iscore::CommandData& d)
{
    m_stream << d.parentKey << d.commandKey << d.data;
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(iscore::CommandData& d)
{
    m_stream >> d.parentKey >> d.commandKey >> d.data;
    checkDelimiter();
}
