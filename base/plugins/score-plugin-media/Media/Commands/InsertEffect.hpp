#pragma once
#include <score/command/Command.hpp>
#include <Dataflow/Commands/CableHelpers.hpp>
#include <Media/Commands/MediaCommandFactory.hpp>
#include <score/model/path/Path.hpp>

namespace Process
{
class EffectModel;
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
                const UuidKey<Process::EffectModel>& effectKind,
                const QString& text,
                std::size_t effectPos);

        void undo(const score::DocumentContext& ctx) const override;
        void redo(const score::DocumentContext& ctx) const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<Effect::ProcessModel> m_model;
        Id<Process::EffectModel> m_id;
        UuidKey<Process::EffectModel> m_effectKind;
        QString m_effect;
        quint64 m_pos{};
};


// MOVEME
class RemoveEffect final : public score::Command
{
           SCORE_COMMAND_DECL(Media::CommandFactoryName(), RemoveEffect, "Remove effect")
    public:
        RemoveEffect(
                const Effect::ProcessModel& model,
                const Process::EffectModel& effect);

        void undo(const score::DocumentContext& ctx) const override;
        void redo(const score::DocumentContext& ctx) const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<Effect::ProcessModel> m_model;
        Id<Process::EffectModel> m_id;
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
                 Id<Process::EffectModel> id,
                 int new_pos);

        void undo(const score::DocumentContext& ctx) const override;
        void redo(const score::DocumentContext& ctx) const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<Effect::ProcessModel> m_model;
        Id<Process::EffectModel> m_id;
        int m_oldPos, m_newPos{};
};
}
}
