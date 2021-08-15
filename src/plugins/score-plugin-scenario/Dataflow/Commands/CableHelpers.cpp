#include "CableHelpers.hpp"

#include <score/model/IdentifierDebug.hpp>

#include <ossia/detail/ptr_set.hpp>

#include <QDebug>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

namespace Dataflow
{
static bool startsWith(const ObjectPath& object, const ObjectPath& parent)
{
  if (object.vec().size() < parent.vec().size())
    return false;

  for (std::size_t i = 0; i < parent.vec().size(); i++)
  {
    if (!(object.vec()[i] == parent.vec()[i]))
      return false;
  }

  return true;
}

std::vector<Process::Cable*>
getCablesInChildObjects(QObjectList objs, const score::DocumentContext& ctx)
{
  std::vector<Process::Cable*> cables;

  ossia::ptr_set<Process::Port*> ports;
  for (auto obj : objs)
  {
    if (auto p = qobject_cast<Process::Port*>(obj))
    {
      ports.insert(p);
    }
    else
    {
      for (auto p : obj->findChildren<Process::Port*>())
      {
        ports.insert(p);
      }
    }
  }

  cables.reserve(0.05 * ports.size()); // totally empiric
  for (auto p : ports)
  {
    for (auto& cbl : p->cables())
    {
      cables.push_back(&cbl.find(ctx));
    }
  }

  return cables;
}
SerializedCables
saveCables(QObjectList objs, const score::DocumentContext& ctx)
{
  SerializedCables cables;

  ossia::ptr_set<Process::Port*> ports;
  for (auto obj : objs)
  {
    if (auto p = qobject_cast<Process::Port*>(obj))
    {
      ports.insert(p);
    }
    else
    {
      for (auto p : obj->findChildren<Process::Port*>())
      {
        ports.insert(p);
      }
    }
  }

  cables.reserve(0.05 * ports.size()); // totally empiric
  for (auto p : ports)
  {
    for (auto& cbl : p->cables())
    {
      Process::Cable& c = cbl.find(ctx);
      cables.push_back({c.id(), c.toCableData()});
    }
  }

  return cables;
}

SerializedCables serializedCablesFromCableJson(
    const ObjectPath& old_path,
    const ObjectPath& new_path,
    const rapidjson::Document::Array& arr)
{
  SerializedCables cables;

  cables.reserve(arr.Size());
  for (const auto& element : arr)
  {
    Process::CableData cd;
    if (element.IsObject() && element.HasMember("ObjectName"))
    {
      Id<Process::Cable> id;
      id <<= JsonValue{element["id"]};
      cd <<= JsonValue{element};

      if (auto& p = cd.source.unsafePath(); startsWith(p, old_path))
        replacePathPart(old_path, new_path, p);

      if (auto& p = cd.sink.unsafePath(); startsWith(p, old_path))
        replacePathPart(old_path, new_path, p);

      cables.emplace_back(std::move(id), std::move(cd));
    }
  }

  return cables;
}
SerializedCables serializedCablesFromCableJson(
    const ObjectPath& old_path,
    const rapidjson::Document::Array& arr)
{
  return serializedCablesFromCableJson(old_path, ObjectPath{}, arr);
}

void removeCables(
    const SerializedCables& cables,
    const score::DocumentContext& ctx)
{
  Scenario::ScenarioDocumentModel& doc
      = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

  for (const auto& cid : cables)
  {
    auto cable_it = doc.cables.find(cid.first);
    if (cable_it != doc.cables.end())
    {
      auto& cable = *cable_it;
      cable.source().find(ctx).removeCable(cable);
      cable.sink().find(ctx).removeCable(cable);
      doc.cables.remove(cid.first);
    }
    else
    {
      qWarning() << "cable " << cid.first << "not found";
    }
  }
}

void restoreCables(
    const SerializedCables& cables,
    const score::DocumentContext& ctx)
{
  Scenario::ScenarioDocumentModel& doc
      = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

  for (const auto& [id, data] : cables)
  {
    if (doc.cables.find(id) == doc.cables.end())
    {
      auto c = new Process::Cable{id, data, &doc};
      doc.cables.add(c);
      c->source().find(ctx).addCable(*c);
      c->sink().find(ctx).addCable(*c);
    }
    else
    {
      qDebug() << "Warning: trying to add existing cable " << id;
    }
  }
}

// TODO keep a single way of doing
void loadCables(
    const ObjectPath& old_path,
    const ObjectPath& new_path,
    Dataflow::SerializedCables& cables,
    const score::DocumentContext& ctx)
{
  for (auto& c : cables)
  {
    if (auto& p = c.second.source.unsafePath(); startsWith(p, old_path))
      replacePathPart(old_path, new_path, p);

    if (auto& p = c.second.sink.unsafePath(); startsWith(p, old_path))
      replacePathPart(old_path, new_path, p);
  }

  Dataflow::restoreCables(cables, ctx);
}

void unstripCables(const ObjectPath& p, SerializedCables& cables)
{
  for (auto& c : cables)
  {
    c.second.source.unsafePath().vec().insert(
        c.second.source.unsafePath().vec().begin(),
        p.vec().begin(),
        p.vec().end());
    c.second.sink.unsafePath().vec().insert(
        c.second.sink.unsafePath().vec().begin(),
        p.vec().begin(),
        p.vec().end());
  }
}

}
