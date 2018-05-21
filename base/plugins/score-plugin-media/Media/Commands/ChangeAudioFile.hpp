#pragma once
#include <Media/Commands/MediaCommandFactory.hpp>
#include <Media/Input/InputModel.hpp>
#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/Path.hpp>
namespace Media
{
namespace Commands
{

class ChangeAudioFile final : public score::Command
{
  SCORE_COMMAND_DECL(
      Media::CommandFactoryName(), ChangeAudioFile, "Change audio file")
public:
  ChangeAudioFile(const Sound::ProcessModel&, const QString& text);

  void undo(const score::DocumentContext& ctx) const override;
  void redo(const score::DocumentContext& ctx) const override;

protected:
  void serializeImpl(DataStreamInput& s) const override;
  void deserializeImpl(DataStreamOutput& s) override;

private:
  Path<Sound::ProcessModel> m_model;
  QString m_old, m_new;
  TimeVal m_olddur;
};

class ChangeStart final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(
      Media::CommandFactoryName(), ChangeStart, "Change start channel")
public:
  ChangeStart(const Sound::ProcessModel& path, int newval)
      : score::PropertyCommand{path, "startChannel",
                               QVariant::fromValue(newval)}
  {
  }
};

class ChangeUpmix final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(
      Media::CommandFactoryName(), ChangeUpmix, "Change upmix channels")
public:
  ChangeUpmix(const Sound::ProcessModel& path, int newval)
      : score::PropertyCommand{path, "upmixChannels",
                               QVariant::fromValue(newval)}
  {
  }
};

class ChangeInputStart final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(
      Media::CommandFactoryName(),
      ChangeInputStart,
      "Change input start channel")
public:
  ChangeInputStart(const Input::ProcessModel& path, int newval)
      : score::PropertyCommand{path, "startChannel",
                               QVariant::fromValue(newval)}
  {
  }
};

class ChangeInputNum final : public score::PropertyCommand
{
  SCORE_COMMAND_DECL(
      Media::CommandFactoryName(), ChangeInputNum, "Change input num channel")
public:
  ChangeInputNum(const Input::ProcessModel& path, int newval)
      : score::PropertyCommand{path, "numChannel", QVariant::fromValue(newval)}
  {
  }
};
}
}
