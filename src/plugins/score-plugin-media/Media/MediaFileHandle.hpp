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
struct RMSData;
class SoundComponentSetup;
struct SCORE_PLUGIN_MEDIA_EXPORT FFMPEGAudioFileHandle final : public QObject
{
public:
  static bool isSupported(const QFile& f);

  FFMPEGAudioFileHandle();
  ~FFMPEGAudioFileHandle() override;

  void load(const QString&, const score::DocumentContext&);

  //! The text passed to the load function
  QString originalFile() const { return m_originalFile; }

  //! Actual filename
  QString fileName() const { return m_fileName; }

  //! Absolute resolved filename
  QString absoluteFileName() const { return m_file; }

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
  void load_ffmpeg();
  void load_drwav();

  friend class SoundComponentSetup;

  struct MmapReader
  {
    QFile file;
    void* data{};
    drwav* wav{};
  };

  struct LibavReader
  {
    AudioDecoder decoder;
    audio_handle handle;
    ossia::small_vector<audio_sample*, 8> data;
  };

  using libav_ptr = std::shared_ptr<LibavReader>;
  using mmap_ptr = std::shared_ptr<MmapReader>;
  using impl_t = std::variant<
    std::monostate,
    mmap_ptr,
    libav_ptr
  >;

  QString m_originalFile;
  QString m_file;
  QString m_fileName;

  impl_t m_impl;

  RMSData* m_rms{};
  int m_sampleRate{};
};
}

Q_DECLARE_METATYPE(std::shared_ptr<Media::FFMPEGAudioFileHandle>)
W_REGISTER_ARGTYPE(std::shared_ptr<Media::FFMPEGAudioFileHandle>)
