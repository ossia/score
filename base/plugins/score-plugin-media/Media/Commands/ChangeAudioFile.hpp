#pragma once
#include <score/command/Command.hpp>
#include <Media/Commands/MediaCommandFactory.hpp>
#include <Media/MediaFileHandle.hpp>
#include <score/model/path/Path.hpp>
namespace Media
{
namespace Sound {
class ProcessModel;
}

namespace Commands
{
class ChangeAudioFile final : public score::Command
{
           SCORE_COMMAND_DECL(Media::CommandFactoryName(), ChangeAudioFile, "Change audio file")
    public:
        ChangeAudioFile(
               const Sound::ProcessModel&,
               const QString& text);

        void undo(const score::DocumentContext& ctx) const override;
        void redo(const score::DocumentContext& ctx) const override;

    protected:
        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        Path<Sound::ProcessModel> m_model;
        QString m_old, m_new;
};
}
}
