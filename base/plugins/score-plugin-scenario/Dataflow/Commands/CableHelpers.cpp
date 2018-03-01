#include "CableHelpers.hpp"

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <ossia/detail/ptr_set.hpp>

namespace Dataflow
{
std::vector<Process::Cable*> getCablesInChildObjects(QObjectList objs, const score::DocumentContext& ctx)
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
      cables.push_back(&cbl.find(ctx));
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
      cables.push_back({c.id(), c.toCableData()});
    }
  }

  return cables;
}

void removeCables(const SerializedCables& cables, const score::DocumentContext& ctx)
{
  Scenario::ScenarioDocumentModel& doc = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

  for(const auto& cid : cables)
  {
    auto cable_it = doc.cables.find(cid.first);
    if(cable_it != doc.cables.end())
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

void restoreCables(const SerializedCables& cables, const score::DocumentContext& ctx)
{
  Scenario::ScenarioDocumentModel& doc = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

  for(const auto& cid : cables)
  {
    if(doc.cables.find(cid.first) == doc.cables.end())
    {
      auto cbl = new Process::Cable{cid.first, cid.second, &doc};
      doc.cables.add(cbl);
      cbl->source().find(ctx).addCable(*cbl);
      cbl->sink().find(ctx).addCable(*cbl);
    }
    else
    {
      qDebug() << "Warning: trying to add existing cable " << cid.first;
    }

  }
}
}
