#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>

#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
template <class T>
class ChangeElementLabel final : public iscore::Command
{
  // No ISCORE_COMMAND here since it's a template.
public:
  const CommandGroupKey& parentKey() const noexcept override
  {
    return ScenarioCommandFactoryName();
  }
  static const CommandKey& static_key() noexcept
  {
    auto name
        = QString("ChangeElementLabel_") + Metadata<ObjectKey_k, T>::get();
    static const CommandKey kagi{std::move(name)};
    return kagi;
  }
  const CommandKey& key() const noexcept override
  {
    return static_key();
  }
  QString description() const override
  {
    return QObject::tr("Change %1 label")
        .arg(Metadata<Description_k, T>::get());
  }

  ChangeElementLabel() = default;

  ChangeElementLabel(const T& obj, QString newLabel)
      : m_path{obj}, m_newLabel{std::move(newLabel)}
  {
    m_oldLabel = obj.metadata().getLabel();
  }

  void undo(const iscore::DocumentContext& ctx) const override
  {
    auto& obj = m_path.find(ctx);
    obj.metadata().setLabel(m_oldLabel);
  }

  void redo(const iscore::DocumentContext& ctx) const override
  {
    auto& obj = m_path.find(ctx);
    obj.metadata().setLabel(m_newLabel);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_path << m_oldLabel << m_newLabel;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_path >> m_oldLabel >> m_newLabel;
  }

private:
  Path<T> m_path;
  QString m_newLabel;
  QString m_oldLabel;
};
}
}

ISCORE_COMMAND_DECL_T(ChangeElementLabel<ConstraintModel>)
ISCORE_COMMAND_DECL_T(ChangeElementLabel<EventModel>)
ISCORE_COMMAND_DECL_T(ChangeElementLabel<TimeSyncModel>)
ISCORE_COMMAND_DECL_T(ChangeElementLabel<StateModel>)
ISCORE_COMMAND_DECL_T(ChangeElementLabel<Process::ProcessModel>)
