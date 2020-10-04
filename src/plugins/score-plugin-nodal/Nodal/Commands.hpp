#pragma once
#include <Dataflow/Commands/CableHelpers.hpp>
#include <Process/ProcessList.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/command/AggregateCommand.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/SerializableHelpers.hpp>

#include <Nodal/CommandFactory.hpp>
#include <Nodal/Process.hpp>

namespace Nodal
{
class Model;

class DropNodesMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(CommandFactoryName(), DropNodesMacro, "Drop nodes")
};

class CreateNode final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), CreateNode, "Create a node")
public:
  CreateNode(
      const Nodal::Model& process,
      QPointF position,
      const UuidKey<Process::ProcessModel>& uuid,
      const QString& dat);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  const Id<Process::ProcessModel>& nodeId() const noexcept { return m_createdNodeId; }

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Nodal::Model> m_path;
  QPointF m_pos;
  UuidKey<Process::ProcessModel> m_uuid;
  QString m_data;

  Id<Process::ProcessModel> m_createdNodeId;
};

class RemoveNode final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), RemoveNode, "Remove a node")
public:
  RemoveNode(const Nodal::Model& p, const Process::ProcessModel& n) : m_path{p}, m_id{n.id()}
  {
    // Save the node
    DataStream::Serializer s1{&m_block};
    s1.readFrom(n);

    m_cables = Dataflow::saveCables(
        {const_cast<Process::ProcessModel*>(&n)}, score::IDocument::documentContext(p));
  }

private:
  void undo(const score::DocumentContext& ctx) const override
  {
    DataStream::Deserializer s{m_block};
    auto& proc = m_path.find(ctx);

    auto& fact = ctx.app.interfaces<Process::ProcessFactoryList>();

    auto node = deserialize_interface(fact, s, ctx, &proc);
    proc.nodes.add(node);

    Dataflow::restoreCables(m_cables, ctx);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    Dataflow::removeCables(m_cables, ctx);

    auto& proc = m_path.find(ctx);
    proc.nodes.remove(m_id);
  }

  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_path << m_id << m_block << m_cables;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_path >> m_id >> m_block >> m_cables;
  }

  Path<Nodal::Model> m_path;
  Id<Process::ProcessModel> m_id;
  QByteArray m_block;
  Dataflow::SerializedCables m_cables;
};

}
