#pragma once
#include <Media/AudioDecoder.hpp>
#include <Media/SndfileDecoder.hpp>

#include <score/tools/std/StringHash.hpp>

#include <ossia/audio/drwav_handle.hpp>
#include <ossia/detail/hash_map.hpp>
#include <ossia/detail/small_vector.hpp>

#include <QFile>

#include <eggs/variant.hpp>
#include <nano_signal_slot.hpp>
#include <score_plugin_media_export.h>

#include <array>
#include <verdigris>
namespace score
{
struct DocumentContext;
}
namespace Media
{
struct RMSData;
class SoundComponentSetup;
#if defined(__clang__)
static constexpr inline float abs_max(float f1, float f2) noexcept
{
  const int mul = (f2 >= 0.f) ? 1 : -1;
  if (f1 < mul * f2)
    return f2;
  else
    return f1;
}
#else
static constexpr inline float abs_max(float f1, float f2) noexcept
{
  return f2 >= 0.f ? f1 < f2 ? f2 : f1 : f1 < -f2 ? f2 : f1;
}
#endif

enum class DecodingMethod
{
  Invalid,
  Mmap,
  Libav,
  Sndfile
};

struct SCORE_PLUGIN_MEDIA_EXPORT AudioFile final : public QObject
{
public:
  static bool isSupported(const QFile& f);

  AudioFile();
  ~AudioFile() override;

  void load(const QString&, const QString&);
  void load(const QString&, const QString&, DecodingMethod d);

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
  bool finishedDecoding() const noexcept { return m_fullyDecoded; }

  const RMSData& rms() const;

  //! Get a copy of the audio array, as 32 bit floats, whatever the input format is
  ossia::audio_array getAudioArray() const;

  Nano::Signal<void()> on_mediaChanged;
  Nano::Signal<void()> on_newData;
  Nano::Signal<void()> on_finishedDecoding;

  void updateSampleRate(int);

  struct MmapReader
  {
    std::shared_ptr<QFile> file;
    void* data{};
    ossia::drwav_handle wav;
  };

  struct RAMReader
  {
    ossia::audio_handle handle;
    ossia::small_vector<ossia::audio_sample*, 8> data;
  };

  struct LibavReader : RAMReader
  {
    explicit LibavReader(int rate) noexcept
        : decoder{rate}
    {
    }
    AudioDecoder decoder;
  };
  struct SndfileReader : RAMReader
  {
    SndfileDecoder decoder;
  };

  using libav_ptr = std::shared_ptr<LibavReader>;
  using mmap_ptr = MmapReader;
  using sndfile_ptr = SndfileReader;
  using impl_t = eggs::variant<mmap_ptr, libav_ptr, sndfile_ptr>;

  struct MmapView
  {
    // Copy for thread-safety reasons
    ossia::drwav_handle wav;
  };

  struct RAMView
  {
    ossia::small_vector<audio_sample*, 8> data;
  };

  struct Handle : impl_t
  {
    using impl_t::impl_t;
    Handle(mmap_ptr&& ptr)
        : impl_t{std::move(ptr)}
    {
    }
    Handle(libav_ptr&& ptr)
        : impl_t{std::move(ptr)}
    {
    }
    Handle(sndfile_ptr&& ptr)
        : impl_t{std::move(ptr)}
    {
    }
    Handle& operator=(mmap_ptr&& ptr)
    {
      ((impl_t&)*this) = std::move(ptr);
      return *this;
    }
    Handle& operator=(libav_ptr&& ptr)
    {
      ((impl_t&)*this) = std::move(ptr);
      return *this;
    }
    Handle& operator=(sndfile_ptr&& ptr)
    {
      ((impl_t&)*this) = std::move(ptr);
      return *this;
    }
  };

  using view_impl_t = eggs::variant<MmapView, RAMView>;
  struct ViewHandle : view_impl_t
  {
    using view_impl_t::view_impl_t;
    ViewHandle(const Handle&);

    void
    frame(int64_t start_frame, ossia::small_vector<float, 8>& out) noexcept;
    void absmax_frame(
        int64_t start_frame,
        int64_t end_frame,
        ossia::small_vector<float, 8>& out) noexcept;
    void minmax_frame(
        int64_t start_frame,
        int64_t end_frame,
        ossia::small_vector<std::pair<float, float>, 8>& out) noexcept;
  };

  // Note : this is a copy, because it's not thread safe.
  ViewHandle handle() const noexcept { return m_impl; }

  const Handle& unsafe_handle() const noexcept { return m_impl; }

  std::optional<double> knownTempo() const noexcept;
private:

  void load_ffmpeg(int rate);
  void load_drwav();
  void load_sndfile();

  friend class SoundComponentSetup;

  QString m_originalFile;
  QString m_file;
  QString m_fileName;

  RMSData* m_rms{};
  int m_sampleRate{};
  bool m_fullyDecoded{};

  Handle m_impl;
};

class SCORE_PLUGIN_MEDIA_EXPORT AudioFileManager final : public QObject
{
public:
  AudioFileManager() noexcept;
  ~AudioFileManager() noexcept;

  static AudioFileManager& instance() noexcept;
  std::shared_ptr<AudioFile>
  get(const QString&, const score::DocumentContext&);

private:
  ossia::fast_hash_map<QString, std::shared_ptr<AudioFile>> m_handles;
};

/**
 * @brief Saves an audio file as .wav (in 32-bit float format)
 */
SCORE_PLUGIN_MEDIA_EXPORT
void writeAudioArrayToFile(const QString& path, const ossia::audio_array& arr, int fs);


std::optional<double> estimateTempo(const AudioFile& file);
std::optional<double> estimateTempo(const QString& filePath);

std::optional<AudioInfo> probe(const QString& path);


}

Q_DECLARE_METATYPE(std::shared_ptr<Media::AudioFile>)
W_REGISTER_ARGTYPE(std::shared_ptr<Media::AudioFile>)
Q_DECLARE_METATYPE(const Media::AudioFile*)
W_REGISTER_ARGTYPE(const Media::AudioFile*)
