#pragma once
#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
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

class ChangeStart final : public score::PropertyCommand
{
    SCORE_COMMAND_DECL(
        Media::CommandFactoryName(),
        ChangeStart,
        "Change start channel")
    public:
      ChangeStart(const Sound::ProcessModel& path, int newval)
    : score::PropertyCommand{path, "startChannel", QVariant::fromValue(newval)}
{
}
};
class ChangeUpmix final : public score::PropertyCommand
{
    SCORE_COMMAND_DECL(
        Media::CommandFactoryName(),
        ChangeUpmix,
        "Change upmix channels")
  public:
    ChangeUpmix(const Sound::ProcessModel& path, int newval)
        : score::PropertyCommand{path, "upmixChannels", QVariant::fromValue(newval)}
    {
    }
};
}
}
