#pragma once
#include <Media/AudioArray.hpp>

#include <ossia/detail/optional.hpp>

#include <QThread>

#include <wobjectdefs.h>

#include <atomic>
#include <vector>
struct AVFrame;
struct SwrContext;

namespace Media
{
using audio_handle = ossia::audio_handle;
using audio_array = ossia::audio_array;
using audio_sample = ossia::audio_sample;

struct AudioInfo
{
  int32_t rate{};
  int64_t channels{};
  int64_t length{};
  int64_t max_arr_length{};
};

class AudioDecoder : public QObject
{
  W_OBJECT(AudioDecoder)

public:
  AudioDecoder();
  ~AudioDecoder();
  ossia::optional<AudioInfo> probe(const QString& path);
  void decode(const QString& path, audio_handle hdl);

  static ossia::optional<std::pair<AudioInfo, audio_array>>
  decode_synchronous(const QString& path);

  static QHash<QString, AudioInfo>& database();

  int32_t sampleRate{};
  std::size_t decoded{};

public:
  void newData() W_SIGNAL(newData);
  void finishedDecoding(audio_handle hdl) W_SIGNAL(finishedDecoding, hdl);

  void startDecode(QString str, audio_handle hdl)
      W_SIGNAL(startDecode, str, hdl);

public:
  void on_startDecode(QString, audio_handle hdl);
  W_SLOT(on_startDecode);

private:
  std::size_t read_length(const QString& path);

  QThread* m_baseThread{};
  QThread m_decodeThread;

  template <typename Decoder>
  void decodeFrame(Decoder dec, audio_array& data, AVFrame& frame);
  void decodeRemaining(audio_array& data);
  std::vector<SwrContext*> resampler;
  void initResample();
};
}

Q_DECLARE_METATYPE(Media::audio_handle)
W_REGISTER_ARGTYPE(Media::audio_handle)
