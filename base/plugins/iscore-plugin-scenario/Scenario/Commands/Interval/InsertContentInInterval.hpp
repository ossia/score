#pragma once
#include <Process/ExpandMode.hpp>
#include <QJsonObject>
#include <QMap>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>

#include <iscore/model/Identifier.hpp>

struct DataStreamInput;
struct DataStreamOutput;
namespace Process
{
class ProcessModel;
}
namespace Scenario
{
class IntervalModel;

namespace Command
{
class InsertContentInInterval final : public iscore::Command
{
  ISCORE_COMMAND_DECL(
      ScenarioCommandFactoryName(),
      InsertContentInInterval,
      "Insert content in a interval")
public:
  InsertContentInInterval(
      QJsonObject&& sourceInterval,
      const IntervalModel& targetInterval,
      ExpandMode mode);

  void undo(const iscore::DocumentContext& ctx) const override;
  void redo(const iscore::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput&) const override;
  void deserializeImpl(DataStreamOutput&) override;

private:
  QJsonObject m_source;
  Path<IntervalModel> m_target;
  ExpandMode m_mode{ExpandMode::GrowShrink};

  QMap<Id<Process::ProcessModel>, Id<Process::ProcessModel>> m_processIds;
};
}
}
