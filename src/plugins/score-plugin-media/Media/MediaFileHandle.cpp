#include "MediaFileHandle.hpp"
#include <Media/RMSData.hpp>

#include <Media/AudioDecoder.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/File.hpp>
#include <Audio/Settings/Model.hpp>
#include <ossia/dataflow/nodes/sound_mmap.hpp>

#include <core/document/Document.hpp>
#include <ossia/detail/apply.hpp>

#include <QFileInfo>

#define DR_WAV_NO_STDIO
#include <dr_wav.h>

namespace Media
{

static int64_t readSampleRate(QFile& file)
{
  bool ok = file.open(QIODevice::ReadOnly);
  if(!ok) {
    return -1;
  }

  auto data = file.map(0, file.size());
  if(!data) {
    return -1;
  }
  auto wav = drwav_open_memory(data, file.size());
  if(!wav) {
    return -1;
  }

  auto sr = wav->sampleRate;
  drwav_close(wav);
  return sr;
}

// TODO if it's smaller than e.g. 1 megabyte, it would be worth
// loading it in memory entirely..
// TODO might make sense to do resampling during execution if it's nott too expensive?
enum class DecodingMethod {
  Invalid,
  Mmap,
  Libav
};

static DecodingMethod needsDecoding(const QString& path, int rate)
{
  if(path.endsWith("wav", Qt::CaseInsensitive))
  {
    QFile f(path);
    auto sr = readSampleRate(f);
    if(sr == rate)
      return DecodingMethod::Mmap;
    else
      return DecodingMethod::Libav;
  }
  else
  {
    return DecodingMethod::Libav;
  }
}

AudioFileHandle::AudioFileHandle()
{
  m_impl = {};
  m_rms = new RMSData{};
}

AudioFileHandle::~AudioFileHandle() {}

void AudioFileHandle::load(
    const QString& path,
    const QString& abspath)
{
  m_originalFile = path;
  m_file = abspath;

  const auto& audioSettings = score::GUIAppContext().settings<Audio::Settings::Model>();
  const auto rate = audioSettings.getRate();

  switch(needsDecoding(m_file, rate))
  {
    case DecodingMethod::Libav:
      load_ffmpeg(rate);
      break;
    case DecodingMethod::Mmap:
      load_drwav();
      break;
    default:
      break;
  }
}

int64_t AudioFileHandle::decodedSamples() const
{
  struct
  {
  int64_t operator()() const noexcept
  { return 0; }
  int64_t operator()(const libav_ptr& r) const noexcept
  { return r->decoder.decoded; }
  int64_t operator()(const mmap_ptr& r) const noexcept
  { return r->wav->totalPCMFrameCount; }
  } _;
  return ossia::apply(_, m_impl);
}

bool AudioFileHandle::isSupported(const QFile& file)
{
  return file.exists()
      && file.fileName().contains(
        QRegExp(".(wav|aif|aiff|flac|ogg|mp3|m4a)", Qt::CaseInsensitive));
}

int64_t AudioFileHandle::samples() const
{
  struct
  {
  int64_t operator()() const noexcept
  { return 0; }
  int64_t operator()(const libav_ptr& r) const noexcept
  { const auto& samples = r->handle->data;  return samples.size() > 0 ? samples[0].size() : 0; }
  int64_t operator()(const mmap_ptr& r) const noexcept
  { return r->wav->totalPCMFrameCount; }
  } _;
  return ossia::apply(_, m_impl);
}

int64_t AudioFileHandle::channels() const
{
  struct
  {
  int64_t operator()() const noexcept
  { return 0; }
  int64_t operator()(const libav_ptr& r) const noexcept
  { return r->handle->data.size(); }
  int64_t operator()(const mmap_ptr& r) const noexcept
  { return r->wav->channels; }
  } _;
  return ossia::apply(_, m_impl);
}

void AudioFileHandle::updateSampleRate(int rate)
{
  switch(needsDecoding(m_file, rate))
  {
    case DecodingMethod::Libav:
      load_ffmpeg(rate);
      break;
    case DecodingMethod::Mmap:
      load_drwav();
      break;
    default:
      break;
  }
}

void AudioFileHandle::load_ffmpeg(int rate)
{
  qDebug() << "AudioFileHandle::load_ffmpeg(): " << m_file << rate;
  // Loading with libav is used :
  // - when resampling is required
  // - when the file is not a .wav
  const auto ptr = std::make_shared<LibavReader>(rate);
  auto& r = *ptr;
  QFile f{m_file};
  if (isSupported(f))
  {
    r.handle = std::make_shared<ossia::audio_data>();

    auto info = AudioDecoder::probe(m_file);
    if(!info)
    {
      m_impl = {};
      return;
    }

    m_rms->load(m_file, info->channels, rate, info->duration());

    if(!m_rms->exists())
    {
      connect(&r.decoder, &AudioDecoder::newData,
              this, [=] {
        const auto& r = *eggs::variants::get<std::shared_ptr<LibavReader>>(m_impl);
        std::vector<gsl::span<const audio_sample>> samples;
        auto& handle = r.handle->data;
        const auto decoded = r.decoder.decoded;

        for(auto& channel : handle)
        {
          samples.emplace_back(channel.data(), gsl::span<ossia::audio_sample>::index_type(decoded));
        }
        m_rms->decode(samples);

        on_newData();
      }, Qt::QueuedConnection);

      connect(&r.decoder, &AudioDecoder::finishedDecoding,
              this, [=] {
        const auto& r = *eggs::variants::get<std::shared_ptr<LibavReader>>(m_impl);
        std::vector<gsl::span<const audio_sample>> samples;
        auto& handle = r.handle->data;
        auto decoded = r.decoder.decoded;

        for(auto& channel : handle)
        {
          samples.emplace_back(channel.data(), gsl::span<ossia::audio_sample>::index_type(decoded));
        }
        m_rms->decodeLast(samples);

        on_finishedDecoding();
      }, Qt::QueuedConnection);
    }

    r.decoder.decode(m_file, r.handle);

    m_sampleRate = rate;

    r.data.resize(r.handle->data.size());
    for (std::size_t i = 0; i < r.handle->data.size(); i++)
      r.data[i] = r.handle->data[i].data();

    QFileInfo fi{f};
    m_fileName = fi.fileName();
    m_impl = std::move(ptr);
  }
  else
  {
    m_impl = {};
  }
  qDebug() << "AudioFileHandle::on_mediaChanged(): " << m_file;
  on_mediaChanged();
}

void AudioFileHandle::load_drwav()
{
  qDebug() << "AudioFileHandle::load_drwav(): " << m_file;

  // Loading with drwav is done when the file can be
  // mmapped directly in to memory.

  auto ptr = std::make_shared<MmapReader>();
  MmapReader& r = *ptr;
  r.file.setFileName(m_file);

  bool ok = r.file.open(QIODevice::ReadOnly);
  if(!ok) {
    m_impl = {};
    on_mediaChanged();
  }

  r.data = r.file.map(0, r.file.size());
  if(!r.data) {
    m_impl = {};
    on_mediaChanged();
  }
  r.wav = reinterpret_cast<ossia::drwav_handle*>(drwav_open_memory(r.data, r.file.size()));
  if(!r.wav) {
    m_impl = {};
    on_mediaChanged();
  }

  m_rms->load(m_file,
              r.wav->channels,
              r.wav->sampleRate,
              TimeVal::fromMsecs(1000. * r.wav->totalPCMFrameCount / r.wav->sampleRate)
  );

  if(!m_rms->exists())
  {
    m_rms->decode(*r.wav);
  }

  QFileInfo fi{r.file};
  m_fileName = fi.fileName();

  m_impl = std::move(ptr);

  m_sampleRate = r.wav->sampleRate;

  on_finishedDecoding();
  on_mediaChanged();
  qDebug() << "AudioFileHandle::on_mediaChanged(): " << m_file;
}

AudioFileHandleManager::AudioFileHandleManager() noexcept
{
  auto& audioSettings = score::GUIAppContext().settings<Audio::Settings::Model>();
  con(audioSettings, &Audio::Settings::Model::RateChanged,
      this, [this] (auto newRate) {
    for(auto& [k, v] : m_handles) {
      v->updateSampleRate(newRate);
    }
  });
}

AudioFileHandleManager::~AudioFileHandleManager() noexcept
{

}

AudioFileHandleManager&AudioFileHandleManager::instance() noexcept
{
  static AudioFileHandleManager m;
  return m;
}

std::shared_ptr<AudioFileHandle> AudioFileHandleManager::get(
    const QString& path,
    const score::DocumentContext& ctx)
{
  // TODO what would be a good garbage collection mechanism ?
  auto abspath = score::locateFilePath(path, ctx);
  if(auto it = m_handles.find(abspath); it != m_handles.end())
  {
    return it->second;
  }
  auto r = std::make_shared<AudioFileHandle>();
  r->load(path, abspath);
  m_handles.insert({abspath, r});
  return r;
}

AudioFileHandle::MmapReader::~MmapReader()
{
  if(wav)
    drwav_close(wav);
}

}
