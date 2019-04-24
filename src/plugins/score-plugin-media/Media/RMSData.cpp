#include <Media/RMSData.hpp>
namespace Media
{
static const constexpr auto rms_buffer_size = 512;
RMSData::RMSData(QString abspath)
{
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
  qDebug() << m_file.fileName();
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

rms_sample_t RMSData::valueAt(int64_t start_frame, int64_t end_frame, int32_t channel) const noexcept
{
  int64_t sum = 0;

  assert(start_frame >= 0);
  assert(end_frame >= 0);
  const auto channels = header->channels;

  const auto start_idx = (start_frame / rms_buffer_size + channel);
  auto end_idx = (end_frame / rms_buffer_size + channel);

  if(start_idx >= samples_count) {
    qDebug() << "bug on start! " << start_idx << samples_count;
    return 0;
  }

  if(end_idx > samples_count) {
    qDebug() << "bug on end! " << end_idx << samples_count;
    end_idx = samples_count;
  }

  rms_sample_t* begin = (data + start_idx);
  rms_sample_t* end = (data + end_idx);

  if(begin == end && start_frame < end_frame) {
    // TODO go fetch the actual samples.
    return *begin;
  }

  for(; begin < end; begin += channels) {
    sum += *begin;
  }
  if(auto n = (end_idx-start_idx) / channels;
     n != 0)
    sum /= n;

  return sum;
}

ossia::small_vector<rms_sample_t, 8> RMSData::frame(int64_t start_frame, int64_t end_frame) const noexcept
{
  ossia::small_vector<rms_sample_t, 8> res;
  res.resize(header->channels);
  ossia::small_vector<int64_t, 8> sum;
  sum.resize(header->channels);

  assert(start_frame >= 0);
  assert(end_frame >= 0);
  const auto channels = header->channels;
  const auto start_idx = (start_frame / rms_buffer_size);
  auto end_idx = (end_frame / rms_buffer_size);

  if(start_idx >= samples_count) {
    qDebug() << "bug on start! " << start_idx << samples_count;
    return res;
  }

  if(end_idx > samples_count) {
    qDebug() << "bug on end! " << end_idx << samples_count;
    end_idx = samples_count;
  }

  rms_sample_t* begin = (data + start_idx);
  rms_sample_t* end = (data + end_idx);

  if(begin == end && start_frame < end_frame) {
    // TODO go fetch the actual samples.
    for(int k = 0; k < channels; k++)
      res[k] += begin[k];
    return res;
  }

  for(; begin < end; begin += channels) {
    for(int k = 0; k < channels; k++)
      sum[k] += begin[k];
  }
  if(auto n = (end_idx-start_idx) / channels;
     n != 0)
    for(int k = 0; k < channels; k++)
      res[k] = sum[k] / n;

  return res;
}

rms_sample_t RMSData::computeChannelRMS(gsl::span<const ossia::audio_sample> chan, int64_t start_idx, int64_t buffer_size)
{
  ossia::audio_sample val = 0.;
  auto begin = chan.begin() + start_idx;
  auto end = begin + buffer_size;

  for(auto it = begin; it < end; ++it)
    val += (*it)*(*it);
  return ossia::clamp(sqrt(val), 0., 1.) * std::numeric_limits<rms_sample_t>::max();

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

  // inexact, must take windowing into account (buffers * ~2?)
  //int64_t total_buffers_available = audio.front().size() / (buffers_size * 2);

  // the first buffer must be windowed with what's before !
  const int64_t len = sizeof(rms_sample_t) * channels;
  rms_sample_t* bytes = reinterpret_cast<rms_sample_t*>(alloca(len));

  int64_t start_idx = (buffers_written + 1) * buffer_size /*- buffer_size / 2 */;
  while(start_idx + buffer_size < max_sample) {
    for(int i = 0; i < channels; i++)
    {
      bytes[i] = computeChannelRMS(audio[i], start_idx, buffer_size) ;
    }
    m_file.write(reinterpret_cast<const char*>(bytes), len);
    start_idx += buffer_size ;/// 2;
    frames_count++;
    samples_count += channels;
  }

  // TODO finish the remainign samples
  newSamples();
}
}
