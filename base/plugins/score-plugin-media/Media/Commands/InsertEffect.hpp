#pragma once
#include <Dataflow/Commands/CableHelpers.hpp>
#include <Media/Commands/MediaCommandFactory.hpp>
#include <Media/Effect/EffectProcessModel.hpp>
#include <Effect/EffectFactory.hpp>
#include <Process/ProcessList.hpp>

#include <score/command/Command.hpp>
#include <score/model/path/Path.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Process
{
class ProcessModel;
class ProcessModel;
class EffectFactory;
}
namespace Media
{
namespace Effect {
class ProcessModel;
}

namespace Commands
{
class InsertEffect final : public score::Command
{
  SCORE_COMMAND_DECL(Media::CommandFactoryName(), InsertEffect, "Insert effect")
  public:
    InsertEffect(
      const Effect::ProcessModel& model,
      const UuidKey<Process::ProcessModel>& effectKind,
      std::size_t effectPos);

    void undo(const score::DocumentContext& ctx) const override;
    void redo(const score::DocumentContext& ctx) const override;

  protected:
    void serializeImpl(DataStreamInput & s) const override;
    void deserializeImpl(DataStreamOutput & s) override;

  private:
    Path<Effect::ProcessModel> m_model;
    Id<Process::ProcessModel> m_id;
    UuidKey<Process::ProcessModel> m_effectKind;
    quint64 m_pos{};
};

template<typename T>
class InsertGenericEffect final : public score::Command
{
  public:
    const CommandGroupKey& parentKey() const noexcept override
    { return Media::CommandFactoryName(); }

    static const CommandKey& static_key() noexcept
    {
      auto name
          = QString("InsertEffect_") + Metadata<ObjectKey_k, T>::get();
      static const CommandKey kagi{std::move(name)};
      return kagi;
    }

    const CommandKey& key() const noexcept override
    { return static_key(); }
    QString description() const override
    { return QObject::tr("Insert %1 effect").arg(Metadata<Description_k, T>::get()); }

    InsertGenericEffect() = default;
    InsertGenericEffect(
      const Effect::ProcessModel& model,
      const QString& path,
      std::size_t effectPos):
      m_model{model},
      m_id{getStrongId(model.effects())},
      m_effect{path},
      m_pos{effectPos}
    {
    }

    void undo(const score::DocumentContext& ctx) const override
    {
      auto& process = m_model.find(ctx);
      if(ossia::find(process.effects(), m_id) != process.effects().end())
        process.removeEffect(m_id);
    }

    void redo(const score::DocumentContext& ctx) const override
    {
      auto& process = m_model.find(ctx);
      auto model = new T{TimeVal::zero(), m_effect, m_id, &process};
      process.insertEffect(model, m_pos);
    }

  protected:
    void serializeImpl(DataStreamInput& s) const override
    { s << m_model << m_id << m_effect << m_pos; }

    void deserializeImpl(DataStreamOutput& s) override
    { s >> m_model >> m_id >> m_effect >> m_pos; }

  private:
    Path<Effect::ProcessModel> m_model;
    Id<Process::ProcessModel> m_id;
    QString m_effect;
    quint64 m_pos{};
};

class RemoveEffect final : public score::Command
{
  SCORE_COMMAND_DECL(Media::CommandFactoryName(), RemoveEffect, "Remove effect")
  public:
    RemoveEffect(
      const Effect::ProcessModel& model,
      const Process::ProcessModel& effect);

    void undo(const score::DocumentContext& ctx) const override;
    void redo(const score::DocumentContext& ctx) const override;

  protected:
    void serializeImpl(DataStreamInput & s) const override;
    void deserializeImpl(DataStreamOutput & s) override;

  private:
    Path<Effect::ProcessModel> m_model;
    Id<Process::ProcessModel> m_id;
    QByteArray m_savedEffect;
    Dataflow::SerializedCables m_cables;
    int m_pos{};
};

class MoveEffect final : public score::Command
{
  SCORE_COMMAND_DECL(Media::CommandFactoryName(), MoveEffect, "Move effect")
  public:
    MoveEffect(
      const Effect::ProcessModel& model,
      Id<Process::ProcessModel> id,
      int new_pos);

    void undo(const score::DocumentContext& ctx) const override;
    void redo(const score::DocumentContext& ctx) const override;

  protected:
    void serializeImpl(DataStreamInput & s) const override;
    void deserializeImpl(DataStreamOutput & s) override;

  private:
    Path<Effect::ProcessModel> m_model;
    Id<Process::ProcessModel> m_id;
    int m_oldPos, m_newPos{};
};
}
}
