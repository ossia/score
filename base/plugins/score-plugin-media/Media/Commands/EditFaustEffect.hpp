#pragma once
#include <score/command/Command.hpp>
#include <Media/Commands/MediaCommandFactory.hpp>
#include <score/model/path/Path.hpp>

namespace Audio
{
namespace Effect {
class FaustEffectModel;
}

namespace Commands
{
class EditFaustEffect final : public score::Command
{
           SCORE_COMMAND_DECL(Media::CommandFactoryName(), EditFaustEffect, "Edit Faust effect")
    public:
        EditFaustEffect(
                const Effect::FaustEffectModel& model,
                const QString& text);

        void undo(const score::DocumentContext& ctx) const override;
        void redo(const score::DocumentContext& ctx) const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<Effect::FaustEffectModel> m_model;
        QString m_old, m_new;

};
}
}
