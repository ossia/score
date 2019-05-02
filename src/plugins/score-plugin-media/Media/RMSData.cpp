#include "RMSData.hpp"

#include <Media/RMSData.hpp>
#include <wobjectimpl.h>
#include <dr_wav.h>

W_OBJECT_IMPL(Media::RMSData)
namespace Media
{
static const constexpr auto rms_buffer_size = 512;
RMSData::RMSData()
{

}

void RMSData::load(QString abspath)
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
    m_file.open(QIODevice::ReadWrite);
    Header h;
    m_file.write((const char*)&h, sizeof(h));

    void* data = m_file.map(0, m_file.size());
    assert(data);
    this->header = reinterpret_cast<Header*>(data);
    this->data = reinterpret_cast<rms_sample_t*>(((char*)data) + sizeof(Header));
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
}

void RMSData::decode(drwav& audio)
{
  // store the data in interleaved format, it's much easier...
  const int64_t channels = audio.channels;
  const int64_t max_frames = audio.totalPCMFrameCount;
  const auto buffer_size = rms_buffer_size;
  header->channels = channels;
  header->bufferSize = buffer_size;

  m_file.seek(m_file.size());

  const int64_t len = sizeof(rms_sample_t) * channels;
  rms_sample_t* bytes = reinterpret_cast<rms_sample_t*>(alloca(len));

  int64_t start_idx = 0;
  while(start_idx < max_frames) {
    computeChannelRMS(audio, bytes, buffer_size);
    m_file.write(reinterpret_cast<const char*>(bytes), len);
    start_idx += buffer_size;
    frames_count++;
    samples_count += channels;
  }

  m_file.flush();
  m_file.waitForBytesWritten(100);
  newData();
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
    qDebug() << "bug on start! " << start_idx << samples_count;
    return sum;
  }

  if(end_idx > samples_count) {
    qDebug() << "bug on end! " << end_idx << samples_count;
    end_idx = samples_count;
  }

  rms_sample_t* begin = (data + start_idx);
  rms_sample_t* end = (data + end_idx);

  if(begin == end && start_frame < end_frame) {
    // TODO go fetch the actual samples.
    for(std::size_t k = 0; k < channels; k++)
      sum[k] = begin[k];

    return sum;
  }

  for(; begin < end; begin += channels) {
    for(std::size_t k = 0; k < channels; k++)
      sum[k] += begin[k];
  }

  const float n = std::max(int64_t(1), (end_idx-start_idx) / channels);
  for(std::size_t k = 0; k < channels; k++)
    sum[k] = sum[k] / n;

  return sum;
}

rms_sample_t RMSData::computeChannelRMS(gsl::span<const ossia::audio_sample> chan, int64_t start_idx, int64_t buffer_size)
{
  ossia::audio_sample val = 0.;
  auto begin = chan.begin() + start_idx;
  auto end = begin + buffer_size;

  for(auto it = begin; it < end; ++it)
    val += (*it)*(*it);
  return ossia::clamp(sqrt(val / buffer_size), 0., 1.) * std::numeric_limits<rms_sample_t>::max();
}



void RMSData::computeChannelRMS(drwav& wav, rms_sample_t* bytes, int64_t buffer_size)
{
  const int channels = wav.channels;
  float* val = (float*)alloca(sizeof(float) * channels);

  float* floats = (float*)alloca(sizeof(float) * buffer_size  * channels);
  auto max = drwav_read_pcm_frames_f32(&wav, buffer_size, floats);

  for(int c = 0; c < channels; c++)
    val[c] = floats[c] * floats[c];

  for(decltype(max) i = 1; i < max; i++)
  {
    for(int c = 0; c < channels; c++)
    {
      const float f = floats[i * channels + c];
      val[c] += f * f;
    }
  }

  for(int c = 0; c < channels; c++)
  {
    bytes[c] = ossia::clamp(sqrt(val[c] / buffer_size), 0., 1.) * std::numeric_limits<rms_sample_t>::max();
  }
}

void RMSData::computeRMS(const std::vector<gsl::span<const ossia::audio_sample> >& audio, int buffer_size)
{
  if(audio.empty())
    return;

  // store the data in interleaved format, it's much easier...
  const int64_t channels = audio.size();
  const int64_t max_sample = audio.front().size();
  header->channels = channels;
  header->bufferSize = buffer_size;

  int64_t buffers_written = (m_file.size() - sizeof(Header)) / channels;
  m_file.seek(m_file.size());

  const int64_t len = sizeof(rms_sample_t) * channels;
  rms_sample_t* bytes = reinterpret_cast<rms_sample_t*>(alloca(len));

  int64_t start_idx = buffers_written * buffer_size;
  while(start_idx + buffer_size < max_sample) {
    for(int i = 0; i < channels; i++)
    {
      bytes[i] = computeChannelRMS(audio[i], start_idx, buffer_size) ;
    }

    m_file.write(reinterpret_cast<const char*>(bytes), len);
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
  const int64_t channels = audio.size();
  const int64_t max_frames = audio.front().size();
  header->channels = channels;
  header->bufferSize = buffer_size;

  int64_t buffers_written = (m_file.size() - sizeof(Header)) / channels;
  m_file.seek(m_file.size());

  const int64_t len = sizeof(rms_sample_t) * channels;
  rms_sample_t* bytes = reinterpret_cast<rms_sample_t*>(alloca(len));

  int64_t start_idx = buffers_written * buffer_size;
  while(start_idx + buffer_size < max_frames) {
    for(int i = 0; i < channels; i++)
    {
      bytes[i] = computeChannelRMS(audio[i], start_idx, buffer_size) ;
    }
    m_file.write(reinterpret_cast<const char*>(bytes), len);
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
    m_file.write(reinterpret_cast<const char*>(bytes), len);
    start_idx += buffer_size ;
    frames_count++;
    samples_count += channels;
  }

  newData();
}
}
