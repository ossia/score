#pragma once
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

struct SCORE_PLUGIN_MEDIA_EXPORT MediaFileHandle final : public QObject
{
public:
  static bool isAudioFile(const QFile& f);

  MediaFileHandle();
  ~MediaFileHandle() override;

  void load(const QString& path, const score::DocumentContext&);

  QString path() const { return m_file; }

  QString fileName() const { return m_fileName; }

  const AudioDecoder& decoder() const { return m_decoder; }

  const audio_array& data() const { return m_hdl->data; }

  audio_handle handle() const { return m_hdl; }

  audio_sample** audioData() const;

  int sampleRate() const { return m_sampleRate; }

  // Number of samples in a channel.
  int64_t samples() const;
  int64_t channels() const;

  bool empty() const { return channels() == 0 || samples() == 0; }

  Nano::Signal<void()> on_mediaChanged;

private:
  QString m_file;
  QString m_fileName;
  AudioDecoder m_decoder;
  audio_handle m_hdl;
  ossia::small_vector<audio_sample*, 8> m_data;
  int m_sampleRate{};
};
}

Q_DECLARE_METATYPE(std::shared_ptr<Media::MediaFileHandle>)
W_REGISTER_ARGTYPE(std::shared_ptr<Media::MediaFileHandle>)
