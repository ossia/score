#pragma once

#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/model/path/Path.hpp>

#include <Scenario/Commands/Constraint/SetRigidity.hpp>

#include <State/Expression.hpp>

namespace Scenario
{
class TimeSyncModel;
namespace Command
{
template <typename Scenario_T>
class RemoveTrigger final : public iscore::Command
{
public:
  const CommandGroupKey& parentKey() const noexcept override
  {
    return CommandFactoryName<Scenario_T>();
  }
  const CommandKey& key() const noexcept override
  {
    return static_key();
  }
  QString description() const override
  {
    return QObject::tr("Remove a trigger");
  }
  static const CommandKey& static_key()
  {
    static const CommandKey kagi{
        QString("RemoveTrigger_") + Metadata<ObjectKey_k, Scenario_T>::get()};
    return kagi;
  }

  RemoveTrigger() = default;

  RemoveTrigger(Path<TimeSyncModel>&& timeSyncPath)
      : m_path{std::move(timeSyncPath)}
  {
  }

  void undo(const iscore::DocumentContext& ctx) const override
  {
    auto& tn = m_path.find(ctx);
    tn.setActive(true);

    for (const auto& cmd : m_cmds)
    {
      cmd.undo(ctx);
    }

    m_cmds.clear();
  }
  void redo(const iscore::DocumentContext& ctx) const override
  {
    auto& tn = m_path.find(ctx);
    tn.setActive(false);

    auto scenar = safe_cast<Scenario_T*>(tn.parent());

    for (const auto& cstrId : constraintsBeforeTimeSync(*scenar, tn.id()))
    {
      m_cmds.emplace_back(scenar->constraint(cstrId), true);
      m_cmds.back().redo(ctx);
    }
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_path;
    s << (int32_t)m_cmds.size();

    for (const auto& cmd : m_cmds)
    {
      s << cmd.serialize();
    }
  }
  void deserializeImpl(DataStreamOutput& s) override
  {
    int32_t n;
    s >> m_path;
    s >> n;
    m_cmds.resize(n);
    for (int i = 0; i < n; i++)
    {
      QByteArray a;
      s >> a;
      m_cmds[i].deserialize(a);
    }
  }

private:
  Path<TimeSyncModel> m_path;
  mutable std::vector<SetRigidity> m_cmds;
};
}
}

#include <Scenario/Process/ScenarioModel.hpp>
ISCORE_COMMAND_DECL_T(Scenario::Command::RemoveTrigger<Scenario::ProcessModel>)

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
ISCORE_COMMAND_DECL_T(Scenario::Command::RemoveTrigger<Scenario::BaseScenario>)
