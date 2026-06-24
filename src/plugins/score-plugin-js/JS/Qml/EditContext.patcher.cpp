#include <Process/Commands/Properties.hpp>
#include <Process/Dataflow/Cable.hpp>
#include <Process/Dataflow/Port.hpp>
#include <Process/Process.hpp>
#include <Process/ProcessFactory.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Commands/CommandAPI.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <JS/Qml/EditContext.hpp>
#include <Nodal/Process.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/Path.hpp>

namespace JS
{

QObjectList EditJsContext::processes(QObject* obj)
{
  QObjectList list;

  if(auto itv = qobject_cast<Scenario::IntervalModel*>(obj))
  {
    for(auto& proc : itv->processes)
      list.push_back(&proc);
  }
  else if(auto nodal = qobject_cast<Nodal::Model*>(obj))
  {
    for(auto& node : nodal->nodes)
      list.push_back(&node);
  }
  return list;
}

QObjectList EditJsContext::cables(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return {};

  // Collect all process IDs in this container
  std::vector<const Process::ProcessModel*> procs;
  if(auto itv = qobject_cast<Scenario::IntervalModel*>(obj))
  {
    for(auto& proc : itv->processes)
      procs.push_back(&proc);
  }
  else if(auto nodal = qobject_cast<Nodal::Model*>(obj))
  {
    for(auto& node : nodal->nodes)
      procs.push_back(&node);
  }
  if(procs.empty())
    return {};

  // Find all cables that connect ports belonging to these processes
  auto& root = score::IDocument::get<Scenario::ScenarioDocumentModel>(doc->document);
  QObjectList list;
  for(auto& cable : root.cables)
  {
    // Check if source or sink port belongs to one of our processes
    try
    {
      auto& src_port = cable.source().find(*doc);
      auto& snk_port = cable.sink().find(*doc);
      auto* src_proc = qobject_cast<Process::ProcessModel*>(src_port.parent());
      auto* snk_proc = qobject_cast<Process::ProcessModel*>(snk_port.parent());

      bool src_in = std::find(procs.begin(), procs.end(), src_proc) != procs.end();
      bool snk_in = std::find(procs.begin(), procs.end(), snk_proc) != procs.end();
      if(src_in || snk_in)
        list.push_back(&cable);
    }
    catch(...)
    {
      continue;
    }
  }
  return list;
}

QObject* EditJsContext::cableSource(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto cable = qobject_cast<Process::Cable*>(obj);
  if(!cable)
    return nullptr;

  try
  {
    return &cable->source().find(*doc);
  }
  catch(...)
  {
    return nullptr;
  }
}

QObject* EditJsContext::cableSink(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return nullptr;
  auto cable = qobject_cast<Process::Cable*>(obj);
  if(!cable)
    return nullptr;

  try
  {
    return &cable->sink().find(*doc);
  }
  catch(...)
  {
    return nullptr;
  }
}

QObject* EditJsContext::cableSourceProcess(QObject* obj)
{
  auto port = cableSource(obj);
  if(!port)
    return nullptr;
  return Process::parentProcess(port);
}

QObject* EditJsContext::cableSinkProcess(QObject* obj)
{
  auto port = cableSink(obj);
  if(!port)
    return nullptr;
  return Process::parentProcess(port);
}

int EditJsContext::portType(QObject* obj)
{
  auto port = qobject_cast<Process::Port*>(obj);
  if(!port)
    return -1;
  return static_cast<int>(port->type());
}

QObject* EditJsContext::portProcess(QObject* obj)
{
  if(!obj)
    return nullptr;
  return Process::parentProcess(obj);
}

bool EditJsContext::isTexturePort(QObject* obj)
{
  auto port = qobject_cast<Process::Port*>(obj);
  if(!port)
    return false;
  return port->type() == Process::PortType::Texture;
}

void EditJsContext::moveNode(QObject* obj, QPointF pos)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto proc = qobject_cast<Process::ProcessModel*>(obj);
  if(!proc)
    return;
  auto [m, _] = macro(*doc);
  m->setProperty<Process::ProcessModel::p_position>(*proc, pos);
}

void EditJsContext::resizeNode(QObject* obj, QSizeF size)
{
  auto doc = ctx();
  if(!doc)
    return;
  auto proc = qobject_cast<Process::ProcessModel*>(obj);
  if(!proc)
    return;
  auto [m, _] = macro(*doc);
  m->setProperty<Process::ProcessModel::p_size>(*proc, size);
}

bool EditJsContext::hasGfxPreview(QObject* obj)
{
  auto proc = qobject_cast<Process::ProcessModel*>(obj);
  if(!proc)
    return false;

  for(auto* outlet : proc->outlets())
  {
    if(outlet->type() == Process::PortType::Texture)
      return true;
  }
  return false;
}

QVariantMap EditJsContext::processInfo(QObject* obj)
{
  auto doc = ctx();
  if(!doc)
    return {};
  auto proc = qobject_cast<Process::ProcessModel*>(obj);
  if(!proc)
    return {};

  auto& facts = doc->app.interfaces<Process::ProcessFactoryList>();
  auto* fact = facts.get(proc->concreteKey());
  if(!fact)
    return {};

  auto desc = fact->descriptor(*proc);
  QVariantMap map;
  map["name"] = desc.prettyName;
  map["category"] = desc.categoryText;
  map["description"] = desc.description;
  map["author"] = desc.author;
  map["tags"] = desc.tags;
  return map;
}

}
