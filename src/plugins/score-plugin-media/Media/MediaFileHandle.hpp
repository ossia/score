#pragma once
#include <Media/AudioArray.hpp>
#include <Media/AudioDecoder.hpp>

#include <ossia/detail/small_vector.hpp>

#include <QFile>

#include <nano_signal_slot.hpp>
#include <score_plugin_media_export.h>
#include <wobjectdefs.h>

#include <array>
namespace score
{
struct DocumentContext;
}
namespace Media
{
// TODO store them in an application-wide cache to prevent loading / unloading
// TODO memmap
struct RMSData;
class SoundComponentSetup;
struct SCORE_PLUGIN_MEDIA_EXPORT FFMPEGAudioFileHandle final : public QObject
{
public:
  static bool isSupported(const QFile& f);

  FFMPEGAudioFileHandle();
  ~FFMPEGAudioFileHandle() override;

  void load(const QString& path, const score::DocumentContext&);

  QString path() const { return m_originalFile; }

  QString fileName() const { return m_fileName; }
/*
  const audio_array& data() const { return m_hdl->data; }

  audio_handle handle() const { return m_hdl; }
*/
  int sampleRate() const { return m_sampleRate; }

  int64_t decodedSamples() const;

  // Number of samples in a channel.
  int64_t samples() const;
  int64_t channels() const;

  bool empty() const { return channels() == 0 || samples() == 0; }

  const RMSData& rms() const { SCORE_ASSERT(m_rms); return *m_rms; }

  Nano::Signal<void()> on_mediaChanged;
  Nano::Signal<void()> on_newData;
  Nano::Signal<void()> on_finishedDecoding;

private:
  friend class SoundComponentSetup;
  const AudioDecoder& decoder() const { return m_decoder; }

  QString m_originalFile;
  QString m_file;
  QString m_fileName;

  RMSData* m_rms{};
  AudioDecoder m_decoder;
  audio_handle m_hdl;
  ossia::small_vector<audio_sample*, 8> m_data;
  int m_sampleRate{};
};
}

Q_DECLARE_METATYPE(std::shared_ptr<Media::FFMPEGAudioFileHandle>)
W_REGISTER_ARGTYPE(std::shared_ptr<Media::FFMPEGAudioFileHandle>)
