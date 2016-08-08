#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "LocalSpecificSettings.hpp"

template <typename T> class Reader;
template <typename T> class Writer;


template<>
void Visitor<Reader<DataStream>>::readFrom_impl(const Engine::Network::LocalSpecificSettings& n)
{
    insertDelimiter();
}

template<>
void Visitor<Writer<DataStream>>::writeTo(Engine::Network::LocalSpecificSettings& n)
{
    checkDelimiter();
}

template<>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Engine::Network::LocalSpecificSettings& n)
{
}

template<>
void Visitor<Writer<JSONObject>>::writeTo(Engine::Network::LocalSpecificSettings& n)
{
}
