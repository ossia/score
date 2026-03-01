#include <JS/Qml/PatcherContext.hpp>

#include <Process/Dataflow/Cable.hpp>
#include <Process/Process.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

#include <Nodal/Process.hpp>

#include <score/document/DocumentInterface.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(JS::PatcherContext)

namespace JS
{

PatcherContext::PatcherContext(
    QObject* container, const score::DocumentContext& ctx, QObject* parent)
    : QObject{parent}
    , m_container{container}
    , m_ctx{ctx}
    , m_editCtx{new EditJsContext{}}
{
  m_editCtx->setParent(this);
  connectToContainer();
}

PatcherContext::~PatcherContext() { }

void PatcherContext::setContainer(QObject* c)
{
  if(c == m_container)
    return;
  disconnectFromContainer();
  m_container = c;
  connectToContainer();
  containerChanged();
  nodesChanged();
  cablesChanged();
}

QObjectList PatcherContext::nodes() const
{
  QObjectList list;

  if(auto itv = qobject_cast<Scenario::IntervalModel*>(m_container))
  {
    for(auto& proc : itv->processes)
      list.push_back(&proc);
  }
  else if(auto nodal = qobject_cast<Nodal::Model*>(m_container))
  {
    for(auto& node : nodal->nodes)
      list.push_back(&node);
  }
  return list;
}

QObjectList PatcherContext::cables() const
{
  // Collect all processes in this container
  std::vector<const Process::ProcessModel*> procs;
  if(auto itv = qobject_cast<Scenario::IntervalModel*>(m_container))
  {
    for(auto& proc : itv->processes)
      procs.push_back(&proc);
  }
  else if(auto nodal = qobject_cast<Nodal::Model*>(m_container))
  {
    for(auto& node : nodal->nodes)
      procs.push_back(&node);
  }
  if(procs.empty())
    return {};

  auto& root = score::IDocument::get<Scenario::ScenarioDocumentModel>(m_ctx.document);
  QObjectList list;
  for(auto& cable : root.cables)
  {
    try
    {
      auto& src_port = cable.source().find(m_ctx);
      auto& snk_port = cable.sink().find(m_ctx);
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

void PatcherContext::setPanOffset(QPointF p)
{
  if(p != m_panOffset)
  {
    m_panOffset = p;
    panOffsetChanged(p);
  }
}

void PatcherContext::setZoom(double z)
{
  if(z != m_zoom)
  {
    m_zoom = z;
    zoomChanged(z);
  }
}

void PatcherContext::connectToContainer()
{
  if(!m_container)
    return;

  if(auto itv = qobject_cast<Scenario::IntervalModel*>(m_container))
  {
    itv->processes.mutable_added.connect<&PatcherContext::on_nodeAdded>(*this);
    itv->processes.removing.connect<&PatcherContext::on_nodeRemoving>(*this);
  }
  else if(auto nodal = qobject_cast<Nodal::Model*>(m_container))
  {
    nodal->nodes.mutable_added.connect<&PatcherContext::on_nodeAdded>(*this);
    nodal->nodes.removing.connect<&PatcherContext::on_nodeRemoving>(*this);
  }

  // Connect to cable changes in the document
  auto& root = score::IDocument::get<Scenario::ScenarioDocumentModel>(m_ctx.document);
  root.cables.mutable_added.connect<&PatcherContext::on_cableAdded>(*this);
  root.cables.removing.connect<&PatcherContext::on_cableRemoving>(*this);
}

void PatcherContext::disconnectFromContainer()
{
  // Disconnect from all Nano signals
  removeAll();
}

void PatcherContext::on_nodeAdded(Process::ProcessModel&)
{
  nodesChanged();
  cablesChanged();
}

void PatcherContext::on_nodeRemoving(const Process::ProcessModel&)
{
  nodesChanged();
  cablesChanged();
}

void PatcherContext::on_cableAdded(Process::Cable&)
{
  cablesChanged();
}

void PatcherContext::on_cableRemoving(const Process::Cable&)
{
  cablesChanged();
}

}
