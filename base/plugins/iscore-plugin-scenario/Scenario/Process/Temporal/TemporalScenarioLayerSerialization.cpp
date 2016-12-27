#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QVector>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelSerialization.hpp>
#include <algorithm>

#include "TemporalScenarioLayerModel.hpp"
#include <Scenario/Process/AbstractScenarioLayerModel.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/serialization/VisitorCommon.hpp>

struct VisitorVariant;
template <typename T>
class Reader;
template <typename T>
class Writer;

template <>
void Visitor<Reader<DataStream>>::read(
    const Scenario::TemporalScenarioLayer& lm)
{
  auto constraints = Scenario::constraintsViewModels(lm);

  m_stream << (int32_t)constraints.size();

  for (auto constraint : constraints)
  {
    m_stream << constraint->model().id();
    readFrom(*constraint);
  }

  insertDelimiter();
}

template <>
void Visitor<Writer<DataStream>>::writeTo(Scenario::TemporalScenarioLayer& lm)
{
  int32_t count;
  m_stream >> count;

  for (; count-- > 0;)
  {
    Id<Scenario::ConstraintModel> id;
    m_stream >> id;
    auto cstr = Scenario::loadConstraintViewModel(*this, &lm, id);
    lm.addConstraintViewModel(cstr);
  }

  checkDelimiter();
}

template <>
void Visitor<Reader<JSONObject>>::readFromConcrete(
    const Scenario::TemporalScenarioLayer& lm)
{
  QJsonArray arr;

  for (auto cstrvm : Scenario::constraintsViewModels(lm))
  {
    auto obj = toJsonObject(*cstrvm);
    obj["ConstraintId"] = toJsonValue(cstrvm->model().id());
    arr.push_back(std::move(obj));
  }

  m_obj["Constraints"] = std::move(arr);
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(Scenario::TemporalScenarioLayer& lm)
{
  QJsonArray arr = m_obj["Constraints"].toArray();

  for (const auto& json_vref : arr)
  {
    Deserializer<JSONObject> deserializer{json_vref.toObject()};

    auto id = fromJsonValue<Id<Scenario::ConstraintModel>>(deserializer.m_obj["ConstraintId"]);
    auto cstrvm = Scenario::loadConstraintViewModel(deserializer, &lm, id);
    lm.addConstraintViewModel(cstrvm);
  }
}
