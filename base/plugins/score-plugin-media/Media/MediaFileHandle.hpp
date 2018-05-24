#pragma once
#include <Media/AudioDecoder.hpp>
#include <wobjectdefs.h>
#include <QFile>
#include <array>
#include <ossia/detail/small_vector.hpp>
#include <score_plugin_media_export.h>
namespace score
{
struct DocumentContext;
}
namespace Media
{
// TODO store them in an application-wide cache to prevent loading / unloading
// TODO memmap

struct SCORE_PLUGIN_MEDIA_EXPORT MediaFileHandle final
    : public QObject
{
public:
  MediaFileHandle();

  void load(const QString& path, const score::DocumentContext&);

  QString path() const
  {
    return m_file;
  }

  QString fileName() const
  {
    return m_fileName;
  }

  const AudioDecoder& decoder() const
  {
    return m_decoder;
  }
  const AudioArray& data() const
  {
    return m_hdl->data;
  }

  AudioSample** audioData() const;

  int sampleRate() const
  {
    return m_sampleRate;
  }

  static bool isAudioFile(const QFile& f);

  // Number of samples in a channel.
  int64_t samples() const;
  int64_t channels() const;

  bool empty() const
  {
    return channels() == 0 || samples() == 0;
  }

  Nano::Signal<void()> on_mediaChanged;

private:
  QString m_file;
  QString m_fileName;
  AudioDecoder m_decoder;
  audio_handle m_hdl;
  ossia::small_vector<AudioSample*, 8> m_data;
  int m_sampleRate{};
};
}
