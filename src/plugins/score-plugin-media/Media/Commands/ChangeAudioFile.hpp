#pragma once
#include <Media/Commands/MediaCommandFactory.hpp>

#include <Media/MediaFileHandle.hpp>
#include <Media/Sound/SoundModel.hpp>

#include <score/command/Command.hpp>
#include <score/command/PropertyCommand.hpp>
#include <score/model/path/Path.hpp>

namespace Media
{

class ChangeAudioFile final : public score::Command
{
  SCORE_COMMAND_DECL(
      Media::CommandFactoryName(),
      ChangeAudioFile,
      "Change audio file")
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
  TimeVal m_olddur{};
  TimeVal m_newdur{};
  TimeVal m_oldloop{};
};
}

PROPERTY_COMMAND_T(
    Media,
    ChangeStart,
    Sound::ProcessModel::p_startChannel,
    "Change start channel")
PROPERTY_COMMAND_T(
    Media,
    ChangeUpmix,
    Sound::ProcessModel::p_upmixChannels,
    "Change upmix channels")
PROPERTY_COMMAND_T(
    Media,
    ChangeStretchMode,
    Sound::ProcessModel::p_stretchMode,
    "Change stretch mode")
PROPERTY_COMMAND_T(
    Media,
    ChangeTempo,
    Sound::ProcessModel::p_nativeTempo,
    "Change file tempo")
SCORE_COMMAND_DECL_T(Media::ChangeStart)
SCORE_COMMAND_DECL_T(Media::ChangeUpmix)
SCORE_COMMAND_DECL_T(Media::ChangeTempo)
SCORE_COMMAND_DECL_T(Media::ChangeStretchMode)
