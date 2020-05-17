#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
template <class T>
class ChangeElementName final : public score::Command
{
  // No SCORE_COMMAND here since it's a template.
public:
  const CommandGroupKey& parentKey() const noexcept override { return CommandFactoryName(); }
  static const CommandKey& static_key() noexcept
  {
    QString name = QString("ChangeElementName_") + Metadata<ObjectKey_k, T>::get();
    static const CommandKey kagi{std::move(name)};
    return kagi;
  }
  const CommandKey& key() const noexcept override { return static_key(); }
  QString description() const override
  {
    return QObject::tr("Change %1 name").arg(Metadata<Description_k, T>::get());
  }

  ChangeElementName() = default;

  ChangeElementName(const T& obj, QString newName) : m_path{obj}, m_newName{std::move(newName)}
  {
    m_oldName = obj.metadata().getName();
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    auto& obj = m_path.find(ctx);
    obj.metadata().setName(m_oldName);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    auto& obj = m_path.find(ctx);
    obj.metadata().setName(m_newName);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override { s << m_path << m_oldName << m_newName; }

  void deserializeImpl(DataStreamOutput& s) override { s >> m_path >> m_oldName >> m_newName; }

private:
  Path<T> m_path;
  QString m_newName;
  QString m_oldName;
};
}
}

SCORE_COMMAND_DECL_T(ChangeElementName<IntervalModel>)
SCORE_COMMAND_DECL_T(ChangeElementName<EventModel>)
SCORE_COMMAND_DECL_T(ChangeElementName<TimeSyncModel>)
SCORE_COMMAND_DECL_T(ChangeElementName<StateModel>)
SCORE_COMMAND_DECL_T(ChangeElementName<Process::ProcessModel>)
