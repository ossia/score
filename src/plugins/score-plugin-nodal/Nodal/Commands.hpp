#pragma once
#include <Dataflow/Commands/CableHelpers.hpp>
#include <Nodal/CommandFactory.hpp>
#include <Nodal/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/command/AggregateCommand.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/plugins/SerializableHelpers.hpp>

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

  const Id<Process::ProcessModel>& nodeId() const noexcept
  {
    return m_createdNodeId;
  }

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
  RemoveNode(const Nodal::Model& p, const Process::ProcessModel& n);

private:
  void undo(const score::DocumentContext& ctx) const override;

  void redo(const score::DocumentContext& ctx) const override;

  void serializeImpl(DataStreamInput& s) const override;

  void deserializeImpl(DataStreamOutput& s) override;

  Path<Nodal::Model> m_path;
  Id<Process::ProcessModel> m_id;
  QByteArray m_block;
  Dataflow::SerializedCables m_cables;
};

class RemoveNodes final
    : public score::AggregateCommand
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      RemoveNodes,
      "Remove nodes")
};

class ReplaceAllNodes final : public score::Command
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ReplaceAllNodes, "Replace all nodes")
public:
  // Expected data format: see Nodal::Model::savePreset
  ReplaceAllNodes(const Nodal::Model& p, const QByteArray& new_data);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Nodal::Model> m_path;
  QByteArray m_old_block, m_new_block;
  Dataflow::SerializedCables m_old_cables, m_new_cables;
};
}
