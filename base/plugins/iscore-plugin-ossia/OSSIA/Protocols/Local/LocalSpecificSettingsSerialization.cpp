#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "LocalSpecificSettings.hpp"

template <typename T> class Reader;
template <typename T> class Writer;


template<>
void Visitor<Reader<DataStream>>::readFrom(const Ossia::LocalSpecificSettings& n)
{
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Ossia::LocalSpecificSettings& n)
{
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom(const Ossia::LocalSpecificSettings& n)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Ossia::LocalSpecificSettings& n)
{
}
