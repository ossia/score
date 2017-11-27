#pragma once
#include <score/command/Command.hpp>
#include <Media/Commands/MediaCommandFactory.hpp>
#include <score/model/path/Path.hpp>

namespace Media
{
namespace Effect {
class EffectModel;
class ProcessModel;
class EffectFactory;
}

namespace Commands
{
class InsertEffect final : public score::Command
{
           SCORE_COMMAND_DECL(Media::CommandFactoryName(), InsertEffect, "Insert effect")
    public:
        InsertEffect(
                const Effect::ProcessModel& model,
                const UuidKey<Effect::EffectFactory>& effectKind,
                const QString& text,
                int effectPos);

        void undo(const score::DocumentContext& ctx) const override;
        void redo(const score::DocumentContext& ctx) const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<Effect::ProcessModel> m_model;
        Id<Effect::EffectModel> m_id;
        UuidKey<Effect::EffectFactory> m_effectKind;
        QString m_effect;
        int m_pos{};
};


// MOVEME
class RemoveEffect final : public score::Command
{
           SCORE_COMMAND_DECL(Media::CommandFactoryName(), RemoveEffect, "Remove effect")
    public:
        RemoveEffect(
                const Effect::ProcessModel& model,
                const Effect::EffectModel& effect);

        void undo(const score::DocumentContext& ctx) const override;
        void redo(const score::DocumentContext& ctx) const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<Effect::ProcessModel> m_model;
        Id<Effect::EffectModel> m_id;
        QByteArray m_savedEffect;
        int m_pos{};
};


class MoveEffect final : public score::Command
{
           SCORE_COMMAND_DECL(Media::CommandFactoryName(), MoveEffect, "Move effect")
    public:
        MoveEffect(
                const Effect::ProcessModel& model,
                 Id<Effect::EffectModel> id,
                 int new_pos);

        void undo(const score::DocumentContext& ctx) const override;
        void redo(const score::DocumentContext& ctx) const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<Effect::ProcessModel> m_model;
        Id<Effect::EffectModel> m_id;
        int m_oldPos, m_newPos{};
};
}
}
