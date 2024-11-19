#include "CableHelpers.hpp"

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/model/IdentifierDebug.hpp>

#include <ossia/detail/ptr_set.hpp>

#include <QDebug>

namespace Dataflow
{
static bool startsWith(const ObjectPath& object, const ObjectPath& parent)
{
  if(object.vec().size() < parent.vec().size())
    return false;

  for(std::size_t i = 0; i < parent.vec().size(); i++)
  {
    if(!(object.vec()[i] == parent.vec()[i]))
      return false;
  }

  return true;
}

std::vector<Process::Cable*>
getCablesInChildObjects(QObjectList objs, const score::DocumentContext& ctx)
{
  std::vector<Process::Cable*> cables;

  ossia::ptr_set<Process::Port*> ports;
  for(auto obj : objs)
  {
    if(auto p = qobject_cast<Process::Port*>(obj))
    {
      ports.insert(p);
    }
    else
    {
      for(auto p : obj->findChildren<Process::Port*>())
      {
        ports.insert(p);
      }
    }
  }

  cables.reserve(0.05 * ports.size()); // totally empiric
  for(auto p : ports)
  {
    for(auto& cbl : p->cables())
    {
      auto* c = &cbl.find(ctx);
      if(!ossia::contains(cables, c))
        cables.push_back(c);
    }
  }

  return cables;
}
SerializedCables saveCables(QObjectList objs, const score::DocumentContext& ctx)
{
  SerializedCables cables;

  ossia::ptr_set<Process::Port*> ports;
  for(auto obj : objs)
  {
    if(auto p = qobject_cast<Process::Port*>(obj))
    {
      ports.insert(p);
    }
    else
    {
      for(auto p : obj->findChildren<Process::Port*>())
      {
        ports.insert(p);
      }
    }
  }

  cables.reserve(0.05 * ports.size()); // totally empiric
  for(auto p : ports)
  {
    for(auto& cbl : p->cables())
    {
      Process::Cable& c = cbl.find(ctx);
      auto it
          = ossia::find_if(cables, [&c](auto& pair) { return pair.first == c.id(); });
      if(it == cables.end())
      {
        cables.push_back({c.id(), c.toCableData()});
      }
    }
  }

  return cables;
}

SerializedCables serializedCablesFromCableJson(
    const ObjectPath& old_path, const ObjectPath& new_path,
    const rapidjson::Document::Array& arr)
{
  SerializedCables cables;

  cables.reserve(arr.Size());
  for(const auto& element : arr)
  {
    Process::CableData cd;
    if(element.IsObject() && element.HasMember("ObjectName"))
    {
      Id<Process::Cable> id;
      id <<= JsonValue{element["id"]};
      cd <<= JsonValue{element};

      if(auto& p = cd.source.unsafePath(); startsWith(p, old_path))
        replacePathPart(old_path, new_path, p);

      if(auto& p = cd.sink.unsafePath(); startsWith(p, old_path))
        replacePathPart(old_path, new_path, p);

      cables.emplace_back(std::move(id), std::move(cd));
    }
  }

  return cables;
}

SerializedCables serializedCablesFromCableJson(
    const ObjectPath& old_path, const rapidjson::Document::Array& arr)
{
  return serializedCablesFromCableJson(old_path, ObjectPath{}, arr);
}

void removeCables(const SerializedCables& cables, const score::DocumentContext& ctx)
{
  Scenario::ScenarioDocumentModel& doc
      = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

  for(const auto& cid : cables)
  {
    auto cable_it = doc.cables.find(cid.first);
    if(cable_it != doc.cables.end())
    {
      auto& cable = *cable_it;
      if(auto c = cable.source().try_find(ctx))
        c->removeCable(cable);

      if(auto c = cable.sink().try_find(ctx))
        c->removeCable(cable);
      doc.cables.remove(cid.first);
    }
    else
    {
      qWarning() << "cable " << cid.first << "not found";
    }
  }
}

void restoreCables(const SerializedCables& cables, const score::DocumentContext& ctx)
{
  Scenario::ScenarioDocumentModel& doc
      = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

  for(const auto& [id, data] : cables)
  {
    if(doc.cables.find(id) == doc.cables.end())
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
    const ObjectPath& old_path, const ObjectPath& new_path,
    Dataflow::SerializedCables& cables, const score::DocumentContext& ctx)
{
  for(auto& c : cables)
  {
    if(auto& p = c.second.source.unsafePath(); startsWith(p, old_path))
      replacePathPart(old_path, new_path, p);

    if(auto& p = c.second.sink.unsafePath(); startsWith(p, old_path))
      replacePathPart(old_path, new_path, p);
  }

  Dataflow::restoreCables(cables, ctx);
}

void unstripCables(const ObjectPath& p, SerializedCables& cables)
{
  for(auto& c : cables)
  {
    c.second.source.unsafePath().vec().insert(
        c.second.source.unsafePath().vec().begin(), p.vec().begin(), p.vec().end());
    c.second.sink.unsafePath().vec().insert(
        c.second.sink.unsafePath().vec().begin(), p.vec().begin(), p.vec().end());
  }
}

static void restoreCables(
    Process::Inlet& new_p, Scenario::ScenarioDocumentModel& doc,
    const score::DocumentContext& ctx, const Dataflow::SerializedCables& cables)
{
  for(auto& cable : new_p.cables())
  {
    SCORE_ASSERT(!cable.unsafePath().vec().empty());
    auto cable_id = cable.unsafePath().vec().back().id();
    auto it = ossia::find_if(
        cables, [cable_id](auto& c) { return c.first.val() == cable_id; });

    SCORE_ASSERT(it != cables.end());
    SCORE_ASSERT(doc.cables.find(it->first) == doc.cables.end());
    {
      auto c = new Process::Cable{it->first, it->second, &doc};
      doc.cables.add(c);
      c->source().find(ctx).addCable(*c);
    }
  }
}
static void restoreCables(
    Process::Outlet& new_p, Scenario::ScenarioDocumentModel& doc,
    const score::DocumentContext& ctx, const Dataflow::SerializedCables& cables)
{
  for(auto& cable : new_p.cables())
  {
    SCORE_ASSERT(!cable.unsafePath().vec().empty());
    auto cable_id = cable.unsafePath().vec().back().id();
    auto it = ossia::find_if(
        cables, [cable_id](auto& c) { return c.first.val() == cable_id; });

    SCORE_ASSERT(it != cables.end());
    SCORE_ASSERT(doc.cables.find(it->first) == doc.cables.end());
    {
      auto c = new Process::Cable{it->first, it->second, &doc};
      doc.cables.add(c);
      c->sink().find(ctx).addCable(*c);
    }
  }
}

void reloadPortsInNewProcess(
    const std::vector<SavedPort>& oldInlets, const std::vector<SavedPort>& oldOutlets,
    const SerializedCables& oldCables, Process::ProcessModel& process,
    const score::DocumentContext& ctx)
{
  // Try an optimistic matching. Type and name must match.
  auto& doc = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

  const std::size_t min_inlets = std::min(oldInlets.size(), process.inlets().size());
  const std::size_t min_outlets = std::min(oldOutlets.size(), process.outlets().size());
  for(std::size_t i = 0; i < min_inlets; i++)
  {
    auto new_p = process.inlets()[i];
    auto& old_p = oldInlets[i];

    if(new_p->type() == old_p.type && new_p->name() == old_p.name)
    {
      new_p->loadData(old_p.data);
      restoreCables(*new_p, doc, ctx, oldCables);
    }
  }

  for(std::size_t i = 0; i < min_outlets; i++)
  {
    auto new_p = process.outlets()[i];
    auto& old_p = oldOutlets[i];

    if(new_p->type() == old_p.type && new_p->name() == old_p.name)
    {
      new_p->loadData(old_p.data);
      restoreCables(*new_p, doc, ctx, oldCables);
    }
  }
}

void reloadPortsInNewProcess(
    const std::vector<SavedPort>& oldInlets, const std::vector<SavedPort>& oldOutlets,
    Process::ProcessModel& process)
{
  // Try an optimistic matching. Type and name must match.
  const std::size_t min_inlets = std::min(oldInlets.size(), process.inlets().size());
  const std::size_t min_outlets = std::min(oldOutlets.size(), process.outlets().size());
  for(std::size_t i = 0; i < min_inlets; i++)
  {
    auto new_p = process.inlets()[i];
    auto& old_p = oldInlets[i];

    if(new_p->type() == old_p.type && new_p->name() == old_p.name)
    {
      new_p->loadData(old_p.data);
    }
  }

  for(std::size_t i = 0; i < min_outlets; i++)
  {
    auto new_p = process.outlets()[i];
    auto& old_p = oldOutlets[i];

    if(new_p->type() == old_p.type && new_p->name() == old_p.name)
    {
      new_p->loadData(old_p.data);
    }
  }
}
}
