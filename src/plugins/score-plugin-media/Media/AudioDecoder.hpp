#pragma once
#include <Media/AudioArray.hpp>
#include <Process/TimeValue.hpp>

#include <ossia/detail/optional.hpp>
#include <ossia/detail/flicks.hpp>

#include <QThread>

#include <atomic>
#include <vector>
#include <verdigris>
struct AVFrame;
struct SwrContext;

namespace Media
{
struct AudioInfo
{
  int32_t rate{};
  int64_t channels{};
  int64_t length{};
  int64_t max_arr_length{};
  double tempo{120.};

  // Duration
  TimeVal duration() const noexcept
  {
    return TimeVal::fromMsecs(1000. * (double(length) / double(rate)) * (tempo / ossia::root_tempo));
  }
};

class AudioDecoder : public QObject
{
  W_OBJECT(AudioDecoder)

public:
  AudioDecoder(int rate);
  ~AudioDecoder();
  static std::optional<AudioInfo> probe(const QString& path);
  void decode(const QString& path, audio_handle hdl);

  static std::optional<std::pair<AudioInfo, audio_array>>
  decode_synchronous(const QString& path, int rate);

  static QHash<QString, AudioInfo>& database();

  int32_t sampleRate{};
  int32_t channels{};
  std::size_t decoded{};

public:
  void newData() W_SIGNAL(newData);
  void finishedDecoding(audio_handle hdl) W_SIGNAL(finishedDecoding, hdl);

  void startDecode(QString str, audio_handle hdl) W_SIGNAL(startDecode, str, hdl);

public:
  void on_startDecode(QString, audio_handle hdl);
  W_SLOT(on_startDecode);

private:
  static double read_length(const QString& path);

  QThread* m_baseThread{};
  QThread m_decodeThread;
  int m_targetSampleRate{};

  template <typename Decoder>
  void decodeFrame(Decoder dec, audio_array& data, AVFrame& frame);

  template <typename Decoder>
  void decodeRemaining(Decoder dec, audio_array& data, AVFrame& frame);
  std::vector<SwrContext*> resampler;
  void initResample();
};
}
