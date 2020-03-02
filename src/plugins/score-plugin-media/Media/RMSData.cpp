#define DR_WAV_IMPLEMENTATION
#define DR_WAV_NO_STDIO
#include <dr_wav.h>
#undef DR_WAV_IMPLEMENTATION
#include "RMSData.hpp"

#include <ossia/detail/math.hpp>
#include <Media/RMSData.hpp>
#include <Media/MediaFileHandle.hpp>
#include <wobjectimpl.h>

#include <QCryptographicHash>
#include <QDir>
#include <QStandardPaths>
W_OBJECT_IMPL(Media::RMSData)
namespace Media
{
static const constexpr auto rms_buffer_size = 64;
RMSData::RMSData()
{

}

void RMSData::load(QString abspath, int channels, int rate, TimeVal duration)
{
  this->header = nullptr;
  this->data = nullptr;
  this->m_exists = false;
  if(m_file.isOpen())
    m_file.close();

  const auto cache = QStandardPaths::standardLocations(QStandardPaths::StandardLocation::CacheLocation);
  if(cache.empty())
    throw std::runtime_error("No cache folder");

  QCryptographicHash h{QCryptographicHash::Sha1};
  h.addData(abspath.toUtf8());
  auto hash = h.result();

  QDir::root().mkpath(cache.first());
  QDir cache_dir{cache.first()};
  cache_dir.mkdir("waveforms");
  cache_dir.cd("waveforms");

  m_file.setFileName(cache_dir.absoluteFilePath(hash.toBase64(QByteArray::Base64UrlEncoding)));

  if(m_file.exists())
  {
    m_file.open(QIODevice::ReadOnly);
    m_exists = true;

    void* data = m_file.map(0, m_file.size());
    assert(data);
    this->header = reinterpret_cast<Header*>(data);
    this->data = reinterpret_cast<rms_sample_t*>(((char*)data) + sizeof(Header));

    frames_count = (m_file.size() - sizeof(Header)) / sizeof(rms_sample_t);
    samples_count = frames_count * header->channels;
  }
  else
  {
    m_ramData.reserve(duration.msec() * 0.001 * rate * channels * sizeof(rms_sample_t) + 1000);
    m_ramBuffer.setBuffer(&m_ramData);
    m_ramBuffer.open(QIODevice::ReadWrite);
    Header h;
    h.channels = channels;
    h.sampleRate = rate;
    h.bufferSize = rms_buffer_size;
    m_ramBuffer.write((const char*)&h, sizeof(h));

    this->header = reinterpret_cast<Header*>(m_ramData.data());
    this->data = reinterpret_cast<rms_sample_t*>(((char*)m_ramData.data()) + sizeof(Header));
  }
}

bool RMSData::exists() const
{
  return m_exists;
}

void RMSData::decode(const std::vector<gsl::span<const ossia::audio_sample> >& audio)
{
  computeRMS(audio, rms_buffer_size);
}

void RMSData::decodeLast(const std::vector<gsl::span<const ossia::audio_sample> >& audio)
{
  computeLastRMS(audio, rms_buffer_size);

  m_file.open(QIODevice::WriteOnly);
  m_file.write(m_ramData);
  m_file.flush();
  m_file.close();
  finishedDecoding();
}

void RMSData::decode(ossia::drwav_handle& audio)
{
  // store the data in interleaved format, it's much easier...
  const int64_t channels = audio.channels();
  if(channels > 0)
  {
    const int64_t max_frames = audio.totalPCMFrameCount();
    constexpr const auto buffer_size = rms_buffer_size;

    const int64_t len = sizeof(rms_sample_t) * channels;
    rms_sample_t* bytes = reinterpret_cast<rms_sample_t*>(alloca(len));

    int64_t start_idx = 0;
    while(start_idx < max_frames) {
      computeChannelRMS(audio, bytes, buffer_size);
      m_ramBuffer.write(reinterpret_cast<const char*>(bytes), len);
      start_idx += buffer_size;
      frames_count++;
      samples_count += channels;
    }
  }

  newData();

  m_file.open(QIODevice::WriteOnly);
  m_file.write(m_ramData);
  m_file.flush();
  m_file.close();
  finishedDecoding();
}

double RMSData::sampleRateRatio(double expectedRate) const noexcept
{
  return header->sampleRate / expectedRate;
}

ossia::small_vector<float, 8> RMSData::frame(int64_t start_frame, int64_t end_frame) const noexcept
{
  ossia::small_vector<float, 8> sum;
  sum.resize(header->channels);

  assert(start_frame >= 0);
  assert(end_frame >= 0);
  const auto channels = header->channels;
  const auto start_idx = (start_frame / rms_buffer_size);
  auto end_idx = (end_frame / rms_buffer_size);

  if(start_idx >= samples_count) {
    //qDebug() << "bug on start! " << start_idx << samples_count;
    return sum;
  }

  if(end_idx > samples_count) {
    //qDebug() << "bug on end! " << end_idx << samples_count;
    end_idx = samples_count;
  }

  rms_sample_t* begin = (data + start_idx);
  rms_sample_t* end = (data + end_idx);

  constexpr float size_factor  = 1. / std::numeric_limits<rms_sample_t>::max();
  if(begin == end && start_frame < end_frame) {
    // Note: we should not enter this case -
    // instead AudioFileHandle::frame would be used
    for(std::size_t k = 0; k < channels; k++)
      sum[k] = begin[k] * size_factor;
    return sum;
  }

  for(; begin < end; begin += channels) {
    for(std::size_t k = 0; k < channels; k++)
    {
      const int64_t val = begin[k];
      sum[k] = abs_max(sum[k], val);
    }
  }

  for(std::size_t k = 0; k < channels; k++)
    sum[k] *= size_factor;

  return sum;
}

rms_sample_t RMSData::computeChannelRMS(gsl::span<const ossia::audio_sample> chan, int64_t start_idx, int64_t buffer_size)
{
  float val = 0.;
  auto begin = chan.begin() + start_idx;
  auto end = begin + buffer_size;

  for(auto it = begin; it < end; ++it)
    val = abs_max(val, *it);
  return val * std::numeric_limits<rms_sample_t>::max();
}



void RMSData::computeChannelRMS(ossia::drwav_handle& wav, rms_sample_t* bytes, int64_t buffer_size)
{
  const int channels = wav.channels();
  float* val = (float*)alloca(sizeof(float) * channels);

  float* floats = (float*)alloca(sizeof(float) * buffer_size  * channels);
  auto max = wav.read_pcm_frames_f32(buffer_size, floats);
  if(max <= 0)
    return;

  for(int c = 0; c < channels; c++)
    val[c] = floats[c];

  for(decltype(max) i = 1; i < max; i++)
  {
    for(int c = 0; c < channels; c++)
    {
      const float f = floats[i * channels + c];
      val[c] = abs_max(val[c], f);
    }
  }

  for(int c = 0; c < channels; c++)
  {
    bytes[c] = val[c] * std::numeric_limits<rms_sample_t>::max();
  }
}


void RMSData::computeRMS(const std::vector<gsl::span<const ossia::audio_sample> >& audio, int buffer_size)
{
  if(audio.empty())
    return;

  // store the data in interleaved format, it's much easier...
  const int64_t channels = audio.size();
  const int64_t max_sample = audio.front().size();

  const int64_t len = sizeof(rms_sample_t) * channels;
  rms_sample_t* bytes = reinterpret_cast<rms_sample_t*>(alloca(len));

  int64_t start_idx = frames_count * buffer_size;
  while(start_idx + buffer_size < max_sample) {
    for(int i = 0; i < channels; i++)
    {
      bytes[i] = computeChannelRMS(audio[i], start_idx, buffer_size) ;
    }

    m_ramBuffer.write(reinterpret_cast<const char*>(bytes), len);
    start_idx += buffer_size ;
    frames_count++;
    samples_count += channels;
  }

  newData();
}

void RMSData::computeLastRMS(const std::vector<gsl::span<const ossia::audio_sample> >& audio, int buffer_size)
{
  if(audio.empty())
    return;

  // store the data in interleaved format, it's much easier...
  const int channels = audio.size();
  const int64_t max_frames = audio.front().size();

  const int64_t len = sizeof(rms_sample_t) * channels;
  rms_sample_t* bytes = reinterpret_cast<rms_sample_t*>(alloca(len));

  int64_t start_idx = frames_count * buffer_size;
  while(start_idx + buffer_size < max_frames) {
    for(int i = 0; i < channels; i++)
    {
      bytes[i] = computeChannelRMS(audio[i], start_idx, buffer_size) ;
    }
    m_ramBuffer.write(reinterpret_cast<const char*>(bytes), len);
    start_idx += buffer_size ;
    frames_count++;
    samples_count += channels;
  }

  if(start_idx < max_frames - 1)
  {
    for(int i = 0; i < channels; i++)
    {
      bytes[i] = computeChannelRMS(audio[i], start_idx, max_frames - start_idx) ;
    }
    m_ramBuffer.write(reinterpret_cast<const char*>(bytes), len);
    start_idx += buffer_size ;
    frames_count++;
    samples_count += channels;
  }

  newData();
}
}
