#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <iscore/command/Command.hpp>

#include <iscore/model/path/Path.hpp>
#include <iscore/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
template <typename T>
class SetExtendedMetadata final : public iscore::Command
{
  // No ISCORE_COMMAND here since it's a template.
public:
  const CommandGroupKey& parentKey() const noexcept override
  {
    return ScenarioCommandFactoryName();
  }
  static const CommandKey& static_key() noexcept
  {
    auto name = QString("ChangeElementExtendedMetadata_")
                + Metadata<ObjectKey_k, T>::get();
    static const CommandKey kagi{std::move(name)};
    return kagi;
  }
  const CommandKey& key() const noexcept override
  {
    return static_key();
  }
  QString description() const override
  {
    return QObject::tr("Change %1 metadata")
        .arg(Metadata<Description_k, T>::get());
  }

  SetExtendedMetadata() = default;

  SetExtendedMetadata(const T& obj, QVariantMap newM)
      : m_path{obj}, m_newMeta{std::move(newM)}
  {
    m_oldMeta = obj.metadata().getExtendedMetadata();
  }

  void undo(const iscore::DocumentContext& ctx) const override
  {
    auto& obj = m_path.find(ctx);
    obj.metadata().setExtendedMetadata(m_oldMeta);
  }

  void redo(const iscore::DocumentContext& ctx) const override
  {
    auto& obj = m_path.find(ctx);
    obj.metadata().setExtendedMetadata(m_newMeta);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_path << m_oldMeta << m_newMeta;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_path >> m_oldMeta >> m_newMeta;
  }

private:
  Path<T> m_path;
  QVariantMap m_newMeta;
  QVariantMap m_oldMeta;
};
}
}

ISCORE_COMMAND_DECL_T(SetExtendedMetadata<IntervalModel>)
ISCORE_COMMAND_DECL_T(SetExtendedMetadata<EventModel>)
ISCORE_COMMAND_DECL_T(SetExtendedMetadata<TimeSyncModel>)
ISCORE_COMMAND_DECL_T(SetExtendedMetadata<StateModel>)
ISCORE_COMMAND_DECL_T(SetExtendedMetadata<Process::ProcessModel>)
