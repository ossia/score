#include <QJsonObject>
#include <QJsonValue>

#include "DummyModel.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONValueVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
void Visitor<Reader<DataStream>>::readFrom_impl(const Dummy::DummyModel& proc)
{
  insertDelimiter();
}

template <>
void Visitor<Writer<DataStream>>::writeTo(Dummy::DummyModel& proc)
{
  checkDelimiter();
}

template <>
void Visitor<Reader<JSONObject>>::readFrom_impl(const Dummy::DummyModel& proc)
{
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(Dummy::DummyModel& proc)
{
}
