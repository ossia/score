#pragma once
#include <Process/Process.hpp>

#include <Media/Metro/MetroMetadata.hpp>
#include <Media/Metro/MetroNode.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <nano_observer.hpp>

#include <score_plugin_media_export.h>

#include <verdigris>

namespace Process
{
class ControlInlet;
class AudioOutlet;
}
namespace Media
{
struct AudioFile;
namespace Metro
{
/**
 * @brief Plays a click on every beat of the bar.
 *
 * The Sound control picks what plays: the two ticks of the user library, a
 * synthesized sound, or the two files of the Accent file and Beat file
 * controls. The files are decoded when they are chosen, outside of the audio
 * thread, so that they are ready when the execution starts.
 */
class SCORE_PLUGIN_MEDIA_EXPORT Model final
    : public Process::ProcessModel
    , public Nano::Observer
{
  SCORE_SERIALIZE_FRIENDS
  PROCESS_METADATA_IMPL(Media::Metro::Model)

  W_OBJECT(Model)

public:
  //! A longer file is not a click: it is not loaded.
  static constexpr double maxFileSeconds = 30.;

  explicit Model(
      const TimeVal& duration, const Id<Process::ProcessModel>& id, QObject* parent);

  ~Model() override;

  template <typename Impl>
  explicit Model(Impl& vis, QObject* parent)
      : Process::ProcessModel{vis, parent}
  {
    vis.writeTo(*this);
    init();
  }

  void init();

  std::unique_ptr<Process::ControlInlet> sound;
  std::unique_ptr<Process::ControlInlet> accentFile;
  std::unique_ptr<Process::ControlInlet> beatFile;
  std::unique_ptr<Process::ControlInlet> volume;
  std::unique_ptr<Process::ControlInlet> accent;
  std::unique_ptr<Process::ControlInlet> subdivision;
  //! A beat_unit: what Subdivision divides.
  std::unique_ptr<Process::ControlInlet> beat;

  std::unique_ptr<Process::AudioOutlet> audio_outlet;
  std::unique_ptr<Process::Outlet> bang_outlet;

  /**
   * @brief The sounds of the kinds that come from files, at the given rate.
   *
   * Classic: the ticks of the user library. Files: the Accent file and the
   * Beat file, either standing for the other when only one is set.
   * Null while nothing is decoded; the execution then plays a built-in sound.
   */
  sound_set_ptr sounds(sound_kind kind, int rate) const;

  //! The Classic kind's ticks, those of the user library, at the given rate;
  //! null when the library does not have them
  static sound_set_ptr classicSounds(int rate);

  //! What sounds() returns changed: a file was chosen or is now decoded.
  //! Starts decoding a sound file, at the rate of the engine and outside of
  //! the calling thread. `path` is looked up as the paths of the document
  //! `context` is in. Null when it is not a sound of at most maxFileSeconds.
  static std::shared_ptr<AudioFile> decodeFile(const QString& path, const QObject& context);

  //! The sounds of the Files kind out of two files; either may be null or
  //! still decoding. Null when neither has anything to play.
  static sound_set_ptr fileSounds(
      const std::shared_ptr<AudioFile>& accent, const std::shared_ptr<AudioFile>& beat,
      int rate);

  //! What the Accent file and Beat file controls hold; null without a file.
  const std::shared_ptr<AudioFile>& accentSound() const noexcept { return m_accentFile; }
  const std::shared_ptr<AudioFile>& beatSound() const noexcept { return m_beatFile; }

  void mapExternalFiles(Process::ExternalFileMap& map) override;

  // E_SIGNAL: connections from other libraries reach it.
  void soundsChanged() E_SIGNAL(SCORE_PLUGIN_MEDIA_EXPORT, soundsChanged)

private:
  void loadFile(std::shared_ptr<AudioFile>& file, const ossia::value& path);
  void reloadFiles();
  void on_decoded();

  std::shared_ptr<AudioFile> m_accentFile;
  std::shared_ptr<AudioFile> m_beatFile;
};
}
}
