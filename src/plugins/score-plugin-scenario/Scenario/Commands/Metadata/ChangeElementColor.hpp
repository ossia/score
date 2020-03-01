#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/model/ColorReference.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Scenario
{
namespace Command
{
template <class T>
class ChangeElementColor final : public score::Command
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
        = QString("ChangeElementColor_") + Metadata<ObjectKey_k, T>::get();
    static const CommandKey kagi{std::move(name)};
    return kagi;
  }
  const CommandKey& key() const noexcept override { return static_key(); }
  QString description() const override
  {
    return QObject::tr("Change %1 color")
        .arg(Metadata<Description_k, T>::get());
  }

  ChangeElementColor() = default;
  ChangeElementColor(const T& obj, score::ColorRef newColor)
      : m_path{obj}, m_newColor{newColor}
  {
    m_oldColor = obj.metadata().getColor();
  }

  void undo(const score::DocumentContext& ctx) const override
  {
    auto& obj = m_path.find(ctx);
    obj.metadata().setColor(m_oldColor);
  }

  void redo(const score::DocumentContext& ctx) const override
  {
    auto& obj = m_path.find(ctx);
    obj.metadata().setColor(m_newColor);
  }

protected:
  void serializeImpl(DataStreamInput& s) const override
  {
    s << m_path << m_oldColor << m_newColor;
  }

  void deserializeImpl(DataStreamOutput& s) override
  {
    s >> m_path >> m_oldColor >> m_newColor;
  }

private:
  Path<T> m_path;
  score::ColorRef m_newColor;
  score::ColorRef m_oldColor;
};
}
}

SCORE_COMMAND_DECL_T(ChangeElementColor<IntervalModel>)
SCORE_COMMAND_DECL_T(ChangeElementColor<EventModel>)
SCORE_COMMAND_DECL_T(ChangeElementColor<TimeSyncModel>)
SCORE_COMMAND_DECL_T(ChangeElementColor<StateModel>)
SCORE_COMMAND_DECL_T(ChangeElementColor<Process::ProcessModel>)
