#pragma once
#include <Nodal/CommandFactory.hpp>
#include <Nodal/Process.hpp>
#include <score/command/AggregateCommand.hpp>
#include <score/command/PropertyCommand.hpp>
#include <Dataflow/Commands/CableHelpers.hpp>

PROPERTY_COMMAND_T(
    Nodal,
    MoveNode,
    Node::p_position,
    "Move node")
SCORE_COMMAND_DECL_T(Nodal::MoveNode)
PROPERTY_COMMAND_T(
    Nodal,
    ResizeNode,
    Node::p_size,
    "Resize node")
SCORE_COMMAND_DECL_T(Nodal::ResizeNode)
namespace Nodal
{
class Model;
class Node;

class DropNodesMacro final : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      DropNodesMacro,
      "Drop nodes")
};

class CreateNode final : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      CreateNode,
      "Create a node")
public:
  CreateNode(
      const Nodal::Model& process,
      QPointF position,
      const UuidKey<Process::ProcessModel>& uuid,
      const QString& dat);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  const Id<Nodal::Node>& nodeId() const noexcept { return m_createdNodeId; }
protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  Path<Nodal::Model> m_path;
  QPointF m_pos;
  UuidKey<Process::ProcessModel> m_uuid;
  QString m_data;

  Id<Nodal::Node> m_createdNodeId;
};




class RemoveNode final : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      RemoveNode,
      "Remove a node")
public:
  RemoveNode(
      const Nodal::Model& p,
      const Nodal::Node& n
      )
  : m_path{p}
  , m_id{n.id()}
  {
    // Save the node
    DataStream::Serializer s1{&m_block};
    s1.readFrom(n);

    m_cables = Dataflow::saveCables({const_cast<Node*>(&n)}, score::IDocument::documentContext(p));
  }

private:
  void undo(const score::DocumentContext& ctx) const override
  {
    DataStream::Deserializer s{m_block};
    auto& proc = m_path.find(ctx);
    auto node = new Node{s, &proc};
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
  Id<Nodal::Node> m_id;
  QByteArray m_block;
  Dataflow::SerializedCables m_cables;
};


}
