#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/tools/std/Optional.hpp>
#include <rapidjson/document.h>
#include <QPointF>
#include <score_plugin_scenario_export.h>
struct DataStreamInput;
struct DataStreamOutput;
namespace Process
{
class ProcessModelFactory;
class LayerFactory;
class ProcessModel;
}

namespace Scenario
{
class IntervalModel;
namespace Command
{
//! Create a new process in a interval
class SCORE_PLUGIN_SCENARIO_EXPORT AddOnlyProcessToInterval final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      AddOnlyProcessToInterval,
      "Add a process")
public:
  AddOnlyProcessToInterval(
      const IntervalModel& cst,
      UuidKey<Process::ProcessModel> process,
      const QString& dat,
      QPointF pos);
  AddOnlyProcessToInterval(
      const IntervalModel& cst,
      Id<Process::ProcessModel> idToUse,
      UuidKey<Process::ProcessModel> process,
      const QString& dat,
      QPointF pos);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void undo(IntervalModel&) const;
  Process::ProcessModel&
  redo(IntervalModel&, const score::DocumentContext& ctx) const;

  const Path<IntervalModel>& intervalPath() const { return m_path; }

  const Id<Process::ProcessModel>& processId() const
  {
    return m_createdProcessId;
  }

  const UuidKey<Process::ProcessModel>& processKey() const
  {
    return m_processName;
  }

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<IntervalModel> m_path;
  UuidKey<Process::ProcessModel> m_processName;
  QString m_data;
  QPointF m_graphpos;

  Id<Process::ProcessModel> m_createdProcessId{};
};

class SCORE_PLUGIN_SCENARIO_EXPORT LoadOnlyProcessInInterval final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      LoadOnlyProcessInInterval,
      "Load a process")
public:
  LoadOnlyProcessInInterval(
      const IntervalModel& cst,
      Id<Process::ProcessModel> idToUse,
      const rapidjson::Value& obj);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void undo(IntervalModel&) const;
  Process::ProcessModel&
  redo(IntervalModel&, const score::DocumentContext& ctx) const;

  const Path<IntervalModel>& intervalPath() const { return m_path; }

  const Id<Process::ProcessModel>& processId() const
  {
    return m_createdProcessId;
  }

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<IntervalModel> m_path;
  Id<Process::ProcessModel> m_createdProcessId{};
  rapidjson::Document m_data;
};

class SCORE_PLUGIN_SCENARIO_EXPORT DuplicateOnlyProcessToInterval final
    : public score::Command
{
  SCORE_COMMAND_DECL(
      CommandFactoryName(),
      DuplicateOnlyProcessToInterval,
      "Duplicate a process")
public:
  DuplicateOnlyProcessToInterval(
      const IntervalModel& cst,
      const Process::ProcessModel&);
  DuplicateOnlyProcessToInterval(
      const IntervalModel& cst,
      Id<Process::ProcessModel> idToUse,
      const Process::ProcessModel&);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

  void undo(IntervalModel&) const;
  Process::ProcessModel&
  redo(IntervalModel&, const score::DocumentContext& ctx) const;

  const Path<IntervalModel>& intervalPath() const { return m_path; }

  const Id<Process::ProcessModel>& processId() const
  {
    return m_createdProcessId;
  }

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<IntervalModel> m_path;
  QByteArray m_processData;
  Id<Process::ProcessModel> m_createdProcessId{};
};
}
}
