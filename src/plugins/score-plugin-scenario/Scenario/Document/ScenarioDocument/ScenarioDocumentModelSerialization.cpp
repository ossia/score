// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "ScenarioDocumentModel.hpp"

#include <Process/Dataflow/Cable.hpp>
#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
#include <Scenario/Document/DisplayedElements/DisplayedElementsProviderList.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <score/model/path/PathSerialization.hpp>

#include <QJsonDocument>

namespace score
{
class DocumentDelegateModel;
} // namespace score
struct VisitorVariant;
template <typename T>
class Reader;
template <typename T>
class Writer;
template <typename model>
class IdentifiedObject;

template <>
void DataStreamReader::read(const Scenario::ScenarioDocumentModel& model)
{
  readFrom(*model.m_baseScenario);

  if (model.cables.empty() && !model.m_savedCables.empty())
    m_stream << QJsonDocument(model.m_savedCables).toBinaryData();
  else
    m_stream << QJsonDocument(toJsonArray(model.cables)).toBinaryData();

  std::vector<Path<Scenario::IntervalModel>> buses;
  for(auto& bus : model.busIntervals)
    buses.emplace_back(*bus);
  m_stream << buses;

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Scenario::ScenarioDocumentModel& model)
{
  model.m_baseScenario = new Scenario::BaseScenario{*this, model.m_context, &model};
  QByteArray arr;
  m_stream >> arr;
  model.m_savedCables = QJsonDocument::fromBinaryData(arr).array();

  auto& ctx = safe_cast<score::Document*>(model.parent()->parent())->context();

  std::vector<Path<Scenario::IntervalModel>> buses;
  m_stream >> buses;
  for(auto& path : buses)
  {
    model.busIntervals.push_back(&path.find(ctx));
  }


  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Scenario::ScenarioDocumentModel& model)
{
  obj["BaseScenario"] = toJsonObject(*model.m_baseScenario);
  obj["Cables"] = toJsonArray(model.cables);

  QJsonArray buses;
  for(auto& bus : model.busIntervals)
    buses.push_back(toJsonObject(Path{*bus}));
  obj["BusIntervals"] = buses;
}

template <>
void JSONObjectWriter::write(Scenario::ScenarioDocumentModel& model)
{
  model.m_baseScenario = new Scenario::BaseScenario(
      JSONObject::Deserializer{obj["BaseScenario"].toObject()}, model.m_context, &model);

  model.m_savedCables = obj["Cables"].toArray();

  auto& ctx = safe_cast<score::Document*>(model.parent()->parent())->context();

  const auto& buses = obj["BusIntervals"].toArray();
  for(const QJsonValue& bus : buses)
  {
    auto path = fromJsonObject<Path<Scenario::IntervalModel>>(bus);
    model.busIntervals.push_back(&path.find(ctx));
  }
}

void Scenario::ScenarioDocumentModel::serialize(
    const VisitorVariant& vis) const
{
  serialize_dyn(vis, *this);
}
