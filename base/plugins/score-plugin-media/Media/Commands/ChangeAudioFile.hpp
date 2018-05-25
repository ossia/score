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

PROPERTY_COMMAND_T(ChangeStart, Sound::ProcessModel::p_startChannel, "Change start channel")
PROPERTY_COMMAND_T(ChangeUpmix, Sound::ProcessModel::p_upmixChannels, "Change upmix channels")
PROPERTY_COMMAND_T(ChangeStartOffset, Sound::ProcessModel::p_startOffset, "Change start offset")
SCORE_COMMAND_DECL_T(Media::Commands::ChangeStart)
SCORE_COMMAND_DECL_T(Media::Commands::ChangeUpmix)
SCORE_COMMAND_DECL_T(Media::Commands::ChangeStartOffset)

PROPERTY_COMMAND_T(ChangeInputStart, Input::ProcessModel::p_startChannel, "Change input start channel")
PROPERTY_COMMAND_T(ChangeInputNum, Input::ProcessModel::p_numChannel, "Change input num channel")
SCORE_COMMAND_DECL_T(Media::Commands::ChangeInputStart)
SCORE_COMMAND_DECL_T(Media::Commands::ChangeInputNum)

}
}
