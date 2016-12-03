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
void Visitor<Reader<DataStream>>::readFrom_impl(
    const Scenario::TemporalScenarioLayer& lm)
{
  auto constraints = constraintsViewModels(lm);

  m_stream << constraints.size();

  for (auto constraint : constraints)
  {
    readFrom(*constraint);
  }

  insertDelimiter();
}

template <>
void Visitor<Writer<DataStream>>::writeTo(Scenario::TemporalScenarioLayer& lm)
{
  int count;
  m_stream >> count;

  for (; count-- > 0;)
  {
    auto cstr = loadConstraintViewModel(*this, &lm);
    lm.addConstraintViewModel(cstr);
  }

  checkDelimiter();
}

template <>
void Visitor<Reader<JSONObject>>::readFrom_impl(
    const Scenario::TemporalScenarioLayer& lm)
{
  QJsonArray arr;

  for (auto cstrvm : constraintsViewModels(lm))
  {
    arr.push_back(toJsonObject(*cstrvm));
  }

  m_obj["Constraints"] = arr;
}

template <>
void Visitor<Writer<JSONObject>>::writeTo(Scenario::TemporalScenarioLayer& lm)
{
  QJsonArray arr = m_obj["Constraints"].toArray();

  for (const auto& json_vref : arr)
  {
    Deserializer<JSONObject> deserializer{json_vref.toObject()};
    auto cstrvm = loadConstraintViewModel(deserializer, &lm);
    lm.addConstraintViewModel(cstrvm);
  }
}
