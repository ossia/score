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


class ChangeStart final :
    public score::PropertyCommand_T<Sound::ProcessModel::p_startChannel>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ChangeStart, "Change start channel")
public:
  using PropertyCommand_T::PropertyCommand_T;
};

class ChangeUpmix final :
    public score::PropertyCommand_T<Sound::ProcessModel::p_upmixChannels>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ChangeUpmix, "Change upmix channels")
  public:
    using PropertyCommand_T::PropertyCommand_T;
};

class ChangeStartOffset final :
    public score::PropertyCommand_T<Sound::ProcessModel::p_startOffset>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ChangeStartOffset, "Change start offset")
  public:
    using PropertyCommand_T::PropertyCommand_T;
};

class ChangeInputStart final :
    public score::PropertyCommand_T<Input::ProcessModel::p_startChannel>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ChangeInputStart, "Change input start channel")
  public:
    using PropertyCommand_T::PropertyCommand_T;
};

class ChangeInputNum final :
    public score::PropertyCommand_T<Input::ProcessModel::p_numChannel>
{
  SCORE_COMMAND_DECL(CommandFactoryName(), ChangeInputNum, "Change input num channel")
  public:
    using PropertyCommand_T::PropertyCommand_T;
};
}
}
