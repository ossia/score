#include <Nodal/Commands.hpp>
#include <score/model/path/PathSerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/document/DocumentContext.hpp>
#include <Process/ProcessList.hpp>

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
  auto fac
      = ctx.app.interfaces<Process::ProcessFactoryList>().get(m_uuid);
  SCORE_ASSERT(fac);
  auto proc = std::unique_ptr<Process::ProcessModel>{fac->make(
        nodal.duration(),
        m_data,
        Id<Process::ProcessModel>{},
        nullptr)};

  SCORE_ASSERT(proc);
  // todo handle these asserts
  auto node = new Node{std::move(proc), m_createdNodeId, &nodal};
  node->setSize({100, 100});
  node->setPosition(m_pos);
  nodal.nodes.add(node);
}

void CreateNode::serializeImpl(DataStreamInput& s) const
{
  s << m_path << m_pos << m_uuid << m_data << m_createdNodeId;
}

void CreateNode::deserializeImpl(DataStreamOutput& s)
{
  s >> m_path >> m_pos >> m_uuid >> m_data >> m_createdNodeId;
}


}
