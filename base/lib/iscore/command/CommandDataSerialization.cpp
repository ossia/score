#include <iscore/serialization/DataStreamVisitor.hpp>
#include <qbytearray.h>

#include "CommandData.hpp"
#include "iscore/plugins/customfactory/StringFactoryKey.hpp"

template <typename T> class Reader;
template <typename T> class Writer;

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
