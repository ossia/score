#include <Nodal/Commands.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/model/EntitySerialization.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/selection/SelectionStack.hpp>
#include <score/tools/IdentifierGeneration.hpp>

namespace Nodal
{

CreateNode::CreateNode(
    const Nodal::Model& nodal,
    QPointF position,
    const UuidKey<Process::ProcessModel>& process,
    const QString& dat)
    : m_path{nodal}
    , m_pos{position}
    , m_uuid{process}
    , m_data{dat}
    , m_createdNodeId{getStrongId(nodal.nodes)}
{
}

void CreateNode::undo(const score::DocumentContext& ctx) const
{
  auto& nodal = m_path.find(ctx);
  nodal.nodes.remove(m_createdNodeId);
}

void CreateNode::redo(const score::DocumentContext& ctx) const
{
  auto& nodal = m_path.find(ctx);
  auto fac = ctx.app.interfaces<Process::ProcessFactoryList>().get(m_uuid);
  SCORE_ASSERT(fac);
  auto proc
      = fac->make(nodal.duration(), m_data, m_createdNodeId, ctx, &nodal);

  SCORE_ASSERT(proc);
  // todo handle these asserts
  proc->setSize({100, 100});
  proc->setPosition(m_pos);
  nodal.nodes.add(proc);
}

void CreateNode::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_pos << m_uuid << m_data << m_createdNodeId;
}

void CreateNode::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_pos >> m_uuid >> m_data >> m_createdNodeId;
}

ReplaceAllNodes::ReplaceAllNodes(const Model& p, const QByteArray& new_data)
    : m_path{p}
    , m_new_block{new_data}
{
  auto& ctx = score::IDocument::documentContext(p);

  // Save all the content of the nodal process
  DataStream::Serializer s1{&m_old_block};
  {
    s1.m_stream << (int32_t)p.nodes.size();
    for (const auto& node : p.nodes)
      s1.readFrom(node);
  }

  m_old_cables = Dataflow::saveCables(
      {const_cast<Nodal::Model*>(&p)},
      ctx);

  auto doc = readJson(new_data);
  JSONWriter wr{doc};
  m_new_cables = JsonValue{wr.obj["Cables"]}.to<Dataflow::SerializedCables>();
  auto& document
      = score::IDocument::get<Scenario::ScenarioDocumentModel>(ctx.document);

  // Note: when saving, the port's associated cables aren't saved ; it's the cable
  // which carry this information.
  for (auto& c : m_new_cables)
  {
    c.first = getStrongId(document.cables);
  }

  // Note: it may not be that type but we don't care as we just need the string
  // without the last element
  auto parent_path = m_path.splitLast<Scenario::IntervalModel>().first.unsafePath();
  Dataflow::unstripCables(parent_path, m_new_cables);
}

void ReplaceAllNodes::undo(const score::DocumentContext& ctx) const
{
  // Remove new cables
  Dataflow::removeCables(m_new_cables, ctx);

  // Remove new nodes
  auto& proc = m_path.find(ctx);
  proc.nodes.clear();

  // Add back the old nodes
  DataStream::Deserializer s{m_old_block};
  auto& pl = ctx.app.interfaces<Process::ProcessFactoryList>();
  {
    int32_t process_count = 0;
    s.m_stream >> process_count;
    for (; process_count-- > 0;)
    {
      auto node = deserialize_interface(pl, s, ctx, &proc);
      if (node)
      {
        proc.nodes.add(node);
      }
      else
      {
        SCORE_TODO;
      }
    }
  }

  // Add back the old cables
  Dataflow::restoreCables(m_old_cables, ctx);
}

void ReplaceAllNodes::redo(const score::DocumentContext& ctx) const
{
  // Remove previous cables
  Dataflow::removeCables(m_old_cables, ctx);

  // Remove previous nodes
  auto& proc = m_path.find(ctx);
  proc.nodes.clear();
  SCORE_ASSERT(proc.nodes.unsafe_map().m_map.size() == 0);
  SCORE_ASSERT(proc.nodes.unsafe_map().m_order.size() == 0);

  // Add new nodes
  auto doc = readJson(m_new_block);

  static auto& pl = ctx.app.interfaces<Process::ProcessFactoryList>();
  JSONWriter wr{doc};
  for (const auto& json_vref : wr.obj["Nodes"].toArray())
  {
    JSONObject::Deserializer deserializer{json_vref};
    auto p = deserialize_interface(pl, deserializer, ctx, &proc);
    if (p)
      proc.nodes.add(p);
    else
      SCORE_TODO;
  }

  // Add new cables
  Dataflow::restoreCables(m_new_cables, ctx);
}

void ReplaceAllNodes::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_old_block << m_new_block << m_old_cables << m_new_cables;
}

void ReplaceAllNodes::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_old_block >> m_new_block >> m_old_cables >> m_new_cables;
}

RemoveNode::RemoveNode(const Model& p, const Process::ProcessModel& n)
    : m_path{p}
    , m_id{n.id()}
{
  // Save the node
  DataStream::Serializer s1{&m_block};
  s1.readFrom(n);

  m_cables = Dataflow::saveCables(
      {const_cast<Process::ProcessModel*>(&n)},
      score::IDocument::documentContext(p));
}

void RemoveNode::undo(const score::DocumentContext& ctx) const
{
  DataStream::Deserializer s{m_block};
  auto& proc = m_path.find(ctx);

  auto& fact = ctx.app.interfaces<Process::ProcessFactoryList>();

  auto node = deserialize_interface(fact, s, ctx, &proc);
  proc.nodes.add(node);

  Dataflow::restoreCables(m_cables, ctx);
}

void RemoveNode::redo(const score::DocumentContext& ctx) const
{
  Dataflow::removeCables(m_cables, ctx);

  auto& proc = m_path.find(ctx);

  ctx.selectionStack.pruneRecursively(&proc);

  proc.nodes.remove(m_id);
}

void RemoveNode::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_id << m_block << m_cables;
}

void RemoveNode::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_id >> m_block >> m_cables;
}

}
