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
class ChangeElementComments final : public score::Command
{
  // No SCORE_COMMAND here since it's a template.
public:
  const CommandGroupKey& parentKey() const noexcept override
  {
    return CommandFactoryName();
  }
  static const CommandKey& static_key() noexcept
  {
    QString name
        = QString("ChangeElementComments_") + Metadata<ObjectKey_k, T>::get();
    static const CommandKey kagi{std::move(name)};
    return kagi;
  }
  const CommandKey& key() const noexcept override { return static_key(); }
  QString description() const override
  {
    return QObject::tr("Change %1 comments")
        .arg(Metadata<Description_k, T>::get());
  }

  ChangeElementComments() = default;

  ChangeElementComments(const T& obj, QString newComments)
      : m_path{obj}, m_newComments{std::move(newComments)}
  {
    m_oldComments = obj.metadata().getComment();
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    auto& obj = m_path.find(ctx);
    obj.metadata().setComment(m_oldComments);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    auto& obj = m_path.find(ctx);
    obj.metadata().setComment(m_newComments);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_path << m_oldComments << m_newComments;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_path >> m_oldComments >> m_newComments;
  }

private:
  Path<T> m_path;
  QString m_oldComments;
  QString m_newComments;
};
}
}

SCORE_COMMAND_DECL_T(ChangeElementComments<IntervalModel>)
SCORE_COMMAND_DECL_T(ChangeElementComments<EventModel>)
SCORE_COMMAND_DECL_T(ChangeElementComments<TimeSyncModel>)
SCORE_COMMAND_DECL_T(ChangeElementComments<StateModel>)
SCORE_COMMAND_DECL_T(ChangeElementComments<Process::ProcessModel>)
