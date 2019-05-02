#pragma once
#include <gsl/span>
#include <Media/AudioArray.hpp>
#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>
#include <QFile>

namespace Media
{

using rms_sample_t = uint16_t;
struct RMSData : public QObject
{
  W_OBJECT(RMSData)
public:
  struct Header
  {
    uint32_t sampleRate{};
    uint32_t bufferSize{};
    uint32_t channels{};
    uint32_t padding{};
  };

  RMSData();

  void load(QString abspath);
  bool exists() const;

  // deinterleaved
  void decode(const std::vector<gsl::span<const ossia::audio_sample>>& audio);
  void decodeLast(const std::vector<gsl::span<const ossia::audio_sample> >& audio);

  // interleaved
  void decode(drwav& audio);

  ossia::small_vector<float, 8> frame(int64_t start_sample, int64_t end_sample) const noexcept;

  int64_t frames_count = 0;
  int64_t samples_count = 0;

  void newData() W_SIGNAL(newData);
  void finishedDecoding() W_SIGNAL(finishedDecoding);

private:
  QFile m_file;
  bool m_exists{false};

  Header* header{};
  rms_sample_t* data{};

  rms_sample_t computeChannelRMS(gsl::span<const ossia::audio_sample> chan, int64_t start_idx, int64_t buffer_size);
  void computeRMS(const std::vector<gsl::span<const ossia::audio_sample>>& audio, int buffer_size);
  void computeLastRMS(const std::vector<gsl::span<const ossia::audio_sample> >& audio, int buffer_size);

  void computeChannelRMS(drwav& wav, rms_sample_t* bytes, int64_t buffer_size);
};

}
