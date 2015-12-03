#pragma once
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

#include <iscore/tools/std/StdlibWrapper.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
template<typename T>
void Visitor<Reader<DataStream>>::readFrom(const StringKey<T>& key)
{
    m_stream << key.toString();
}

template<typename T>
void Visitor<Writer<DataStream>>::writeTo(StringKey<T>& key)
{
    m_stream >> key.toString();
}

template<typename T>
void Visitor<Reader<JSONValue>>::readFrom(const StringKey<T>& key)
{
    val = QString::fromStdString(key.toString());
}

template<typename T>
void Visitor<Writer<JSONValue>>::writeTo(StringKey<T>& key)
{
    key.toString() = val.toString().toStdString();
}
