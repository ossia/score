#include "MediaFileHandle.hpp"
#include <Media/RMSData.hpp>

#include <Media/AudioDecoder.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/File.hpp>
#include <Audio/Settings/Model.hpp>

#include <core/document/Document.hpp>

#include <QFileInfo>
namespace Media
{

AudioFileHandle::AudioFileHandle()
{
  m_impl = std::monostate{};
  m_rms = new RMSData{};
}

AudioFileHandle::~AudioFileHandle() {}

void AudioFileHandle::load(
    const QString& path,
    const QString& abspath)
{
  m_originalFile = path;
  m_file = abspath;
  // TODO if it's smaller than e.g. 1 megabyte, it would be worth
  // loading it in memory entirely..
  if(m_file.endsWith("wav", Qt::CaseInsensitive))
  {
    load_drwav();
  }
  else
  {
    load_ffmpeg();
  }
}

int64_t AudioFileHandle::decodedSamples() const
{
  struct
  {
  int64_t operator()(const std::monostate& r) const noexcept
  { return 0; }
  int64_t operator()(const libav_ptr& r) const noexcept
  { return r->decoder.decoded; }
  int64_t operator()(const mmap_ptr& r) const noexcept
  { return r->wav->totalPCMFrameCount; }
  } _;
  return std::visit(_, m_impl);
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
  int64_t operator()(const std::monostate& r) const noexcept
  { return 0; }
  int64_t operator()(const libav_ptr& r) const noexcept
  { const auto& samples = r->handle->data;  return samples.size() > 0 ? samples[0].size() : 0; }
  int64_t operator()(const mmap_ptr& r) const noexcept
  { return r->wav->totalPCMFrameCount; }
  } _;
  return std::visit(_, m_impl);
}

int64_t AudioFileHandle::channels() const
{
  struct
  {
  int64_t operator()(const std::monostate& r) const noexcept
  { return 0; }
  int64_t operator()(const libav_ptr& r) const noexcept
  { return r->handle->data.size(); }
  int64_t operator()(const mmap_ptr& r) const noexcept
  { return r->wav->channels; }
  } _;
  return std::visit(_, m_impl);
}

void AudioFileHandle::load_ffmpeg()
{
  auto ptr = std::make_shared<LibavReader>();
  auto& r = *ptr;
  QFile f{m_file};
  if (isSupported(f))
  {
    r.handle = std::make_shared<ossia::audio_data>();
    m_rms->load(m_file);

    if(!m_rms->exists())
    {
      connect(&r.decoder, &AudioDecoder::newData,
              this, [=] {
        const auto& r = *std::get<std::shared_ptr<LibavReader>>(m_impl);
        std::vector<gsl::span<const audio_sample>> samples;
        auto& handle = r.handle->data;
        auto decoded = r.decoder.decoded;

        for(auto& channel : handle)
        {
          samples.emplace_back(channel.data(), gsl::span<ossia::audio_sample>::index_type(decoded));
        }
        m_rms->decode(samples);

        on_newData();
      }, Qt::QueuedConnection);

      connect(&r.decoder, &AudioDecoder::finishedDecoding,
              this, [=] {
        const auto& r = *std::get<std::shared_ptr<LibavReader>>(m_impl);
        std::vector<gsl::span<const audio_sample>> samples;
        auto& handle = r.handle->data;
        auto decoded = r.decoder.decoded;

        for(auto& channel : handle)
        {
          samples.emplace_back(channel.data(), gsl::span<ossia::audio_sample>::index_type(decoded));
        }
        m_rms->decodeLast(samples);
        m_rms->finishedDecoding();

        on_finishedDecoding();
      }, Qt::QueuedConnection);
    }

    r.decoder.decode(m_file, r.handle);

    m_sampleRate = 44100; // TODO for now everything is reencoded

    r.data.resize(r.handle->data.size());
    for (std::size_t i = 0; i < r.handle->data.size(); i++)
      r.data[i] = r.handle->data[i].data();

    QFileInfo fi{f};
    m_fileName = fi.fileName();
    m_impl = std::move(ptr);
  }
  else
  {
    m_impl = std::monostate{};
  }
  on_mediaChanged();
}

void AudioFileHandle::load_drwav()
{
  auto ptr = std::make_shared<MmapReader>();
  MmapReader& r = *ptr;
  r.file.setFileName(m_file);

  bool ok = r.file.open(QIODevice::ReadOnly);
  if(!ok) {
    m_impl = std::monostate{};
    on_mediaChanged();
  }

  r.data = r.file.map(0, r.file.size());
  if(!r.data) {
    m_impl = std::monostate{};
    on_mediaChanged();
  }
  r.wav = drwav_open_memory(r.data, r.file.size());
  if(!r.wav) {
    m_impl = std::monostate{};
    on_mediaChanged();
  }

  m_rms->load(m_file);
  if(!m_rms->exists())
  {
    m_rms->decode(*r.wav);
    m_rms->finishedDecoding();
  }

  QFileInfo fi{r.file};
  m_fileName = fi.fileName();

  m_impl = std::move(ptr);

  m_sampleRate = 44100; // TODO execution won't work for other sample rates for now

  on_mediaChanged();

}

AudioFileHandleManager::AudioFileHandleManager() noexcept
{
  auto& audioSettings = score::GUIAppContext().settings<Audio::Settings::Model>();
  con(audioSettings, &Audio::Settings::Model::RateChanged,
      this, [] {
    // TODO recompute the Libav ones and small ones
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

}
