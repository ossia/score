#pragma once

#include <Scenario/Commands/Interval/SetRigidity.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/TimeSync/TimeSyncModel.hpp>
#include <State/Expression.hpp>

#include <score/command/Command.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/path/Path.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace Scenario
{
class TimeSyncModel;
namespace Command
{
template <typename Scenario_T>
class AddTrigger final : public score::Command
{
public:
  const CommandGroupKey& parentKey() const noexcept override
  {
    return ::CommandFactoryName<Scenario_T>();
  }
  const CommandKey& key() const noexcept override { return static_key(); }
  QString description() const override { return QObject::tr("Add a trigger"); }
  static const CommandKey& static_key() noexcept
  {
    static const CommandKey kagi{
        QString("AddTrigger_") + Metadata<ObjectKey_k, Scenario_T>::get()};
    return kagi;
  }

  AddTrigger() = default;

  AddTrigger(const TimeSyncModel& tn) : m_path{tn}
  {
    Scenario_T* scenar = safe_cast<Scenario_T*>(tn.parent());
    for (const auto& cstrId : intervalsBeforeTimeSync(*scenar, tn.id()))
    {
      m_cmds.emplace_back(scenar->interval(cstrId), false);
    }
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    auto& tn = m_path.find(ctx);
    tn.setActive(false);

    for (const auto& cmd : m_cmds)
    {
      cmd.undo(ctx);
    }
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    auto& tn = m_path.find(ctx);
    tn.setActive(true);

    for (const auto& cmd : m_cmds)
    {
      cmd.redo(ctx);
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
  std::vector<SetRigidity> m_cmds;
};
}
}

#include <Scenario/Process/ScenarioModel.hpp>
SCORE_COMMAND_DECL_T(Scenario::Command::AddTrigger<Scenario::ProcessModel>)

#include <Scenario/Document/BaseScenario/BaseScenario.hpp>
SCORE_COMMAND_DECL_T(Scenario::Command::AddTrigger<Scenario::BaseScenario>)
