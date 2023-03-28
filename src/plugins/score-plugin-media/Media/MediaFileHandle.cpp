#include "MediaFileHandle.hpp"

#include <Media/AudioDecoder.hpp>
#include <Media/RMSData.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/apply.hpp>
#include <ossia/detail/ssize.hpp>

#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
#include <QRegularExpression>
#endif

#include <Audio/Settings/Model.hpp>

#include <ossia/detail/libav.hpp>

#include <QDebug>
#include <QFileInfo>

namespace Media
{

// TODO if it's smaller than e.g. 1 megabyte, it would be worth
// loading it in memory entirely..
// TODO might make sense to do resampling during execution if it's not too
// expensive?
static DecodingMethod needsDecoding(const QString& path, int rate)
{
  if(path.endsWith("wav", Qt::CaseInsensitive)
     || path.endsWith("w64", Qt::CaseInsensitive))
  {
    const auto& info = probe(path);

    if(info && info->fileRate == rate)
      return DecodingMethod::Mmap;
    else
      return DecodingMethod::Libav;
  }
  else if(
      path.endsWith("aiff", Qt::CaseInsensitive)
      || path.endsWith("aif", Qt::CaseInsensitive))
  {
    const auto& info = probe(path);
    if(info && info->fileRate == rate)
      return DecodingMethod::Sndfile;
    else
      return DecodingMethod::Libav;
  }
  else
  {
    return DecodingMethod::Libav;
  }
}

AudioFile::AudioFile()
{
  m_impl = Handle{};
  m_rms = new RMSData{};
}

AudioFile::~AudioFile()
{
  delete m_rms;
}

void AudioFile::load(DecodingSetup opt)
{
  m_originalFile = opt.filePath;
  m_file = opt.absoluteFilePath;
  m_track = opt.track;

  const auto& audioSettings = score::GUIAppContext().settings<Audio::Settings::Model>();
  const auto rate = audioSettings.getRate();

  if(m_track != -1)
    opt.method = DecodingMethod::LibavStream;
  else if(opt.method == DecodingMethod::Invalid)
    opt.method = needsDecoding(m_file, rate);

  switch(opt.method)
  {
    case DecodingMethod::Libav:
      load_libav(rate);
      break;
    case DecodingMethod::Mmap:
      load_drwav();
      break;
    case DecodingMethod::Sndfile:
      load_sndfile();
      break;
    case DecodingMethod::LibavStream:
      // FIXME
      if(m_track == -1)
        m_track = 0;
      load_libav_stream();
      break;
    default:
      break;
  }
}

int64_t AudioFile::decodedSamples() const
{
  struct
  {
    int64_t operator()(ossia::monostate) const noexcept { return 0; }
    int64_t operator()(const libav_ptr& r) const noexcept { return r->decoder.decoded; }
    int64_t operator()(const libav_stream_ptr& r) const noexcept { return r.samples; }
    int64_t operator()(const sndfile_ptr& r) const noexcept { return r.decoder.decoded; }
    int64_t operator()(const mmap_ptr& r) const noexcept
    {
      return r.wav.totalPCMFrameCount();
    }
  } _;
  return ossia::apply(_, m_impl);
}

bool AudioFile::isSupported(const QFile& file)
{
  constexpr auto rex = ".(wav|mp3|m4a|ogg|flac|aif|aiff|w64|ape|wv|wma)";
  return file.exists()
         && file.fileName().contains(
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
             QRegularExpression(rex, QRegularExpression::CaseInsensitiveOption)
#else
             QRegExp(rex, Qt::CaseInsensitive)
#endif
         );
}

int64_t AudioFile::samples() const
{
  struct
  {
    int64_t operator()(ossia::monostate) const noexcept { return 0; }
    int64_t operator()(const libav_ptr& r) const noexcept
    {
      const auto& samples = r->handle->data;
      return samples.size() > 0 ? samples[0].size() : 0;
    }
    int64_t operator()(const libav_stream_ptr& r) const noexcept { return r.samples; }
    int64_t operator()(const sndfile_ptr& r) const noexcept
    {
      const auto& samples = r.handle->data;
      return samples.size() > 0 ? samples[0].size() : 0;
    }
    int64_t operator()(const mmap_ptr& r) const noexcept
    {
      return r.wav.totalPCMFrameCount();
    }
  } _;
  return ossia::apply(_, m_impl);
}

int64_t AudioFile::channels() const
{
  struct
  {
    int64_t operator()(ossia::monostate) const noexcept { return 0; }
    int64_t operator()(const libav_ptr& r) const noexcept
    {
      return r->handle->data.size();
    }
    int64_t operator()(const libav_stream_ptr& r) const noexcept { return r.channels; }
    int64_t operator()(const sndfile_ptr& r) const noexcept
    {
      return r.handle->data.size();
    }
    int64_t operator()(const mmap_ptr& r) const noexcept { return r.wav.channels(); }
  } _;
  return ossia::apply(_, m_impl);
}

const RMSData& AudioFile::rms() const
{
  SCORE_ASSERT(m_rms);
  return *m_rms;
}

std::optional<double> AudioFile::knownTempo() const noexcept
{
  auto& db = AudioDecoder::database();
  if(auto it = db.find(this->m_file); it != db.end())
  {
    return it->tempo;
  }
  return {};
}

AudioFileManager::AudioFileManager() noexcept
{
  auto& audioSettings = score::GUIAppContext().settings<Audio::Settings::Model>();
  con(audioSettings, &Audio::Settings::Model::RateChanged, this,
      [this](auto newRate) { m_handles.clear(); });
}

AudioFileManager::~AudioFileManager() noexcept { }

AudioFileManager& AudioFileManager::instance() noexcept
{
  static AudioFileManager m;
  return m;
}

std::shared_ptr<AudioFile>
AudioFileManager::get(const QString& path, int stream, const score::DocumentContext& ctx)
{
  // TODO what would be a good garbage collection mechanism ?
  auto abspath = score::locateFilePath(path, ctx);
  StreamInfo k{abspath, stream};
  if(auto it = m_handles.find(k); it != m_handles.end())
  {
    return it->second;
  }

  auto r = std::make_shared<AudioFile>();
  r->load({path, abspath, DecodingMethod::Invalid, stream});
  m_handles.insert({k, r});
  return r;
}

std::shared_ptr<AudioFile> AudioFileManager::get(const QString& abspath, int stream)
{
  // TODO what would be a good garbage collection mechanism ?
  StreamInfo k{abspath, stream};
  if(auto it = m_handles.find(k); it != m_handles.end())
  {
    return it->second;
  }

  auto r = std::make_shared<AudioFile>();
  r->load({abspath, abspath, DecodingMethod::Invalid, stream});
  m_handles.insert({k, r});
  return r;
}

AudioFile::ViewHandle::ViewHandle(const AudioFile::Handle& handle)
{
  struct
  {
    view_impl_t& self;
    void operator()(ossia::monostate) const noexcept { }
    void operator()(const libav_ptr& r) const noexcept { self = RAMView{r->data}; }
    void operator()(const libav_stream_ptr& r) const noexcept
    {
      auto ptr = std::make_shared<ossia::libav_handle>();
      ptr->open(r.path, r.stream, 0);
      if(ptr)
      {
        self = StreamView{std::move(ptr)};
      }
    }
    void operator()(const sndfile_ptr& r) const noexcept { self = RAMView{r.data}; }
    void operator()(const mmap_ptr& r) const noexcept
    {
      if(r.wav)
      {
        self = MmapView{r.wav};
      }
    }
  } _{*this};

  ossia::apply(_, handle);
}

ossia::audio_array AudioFile::getAudioArray() const
{
  struct
  {
    int64_t frames{};
    ossia::audio_array out;

    void operator()(ossia::monostate) noexcept { }
    void operator()(const Media::AudioFile::RAMView& av) noexcept
    {
      const int channels = av.data.size();
      out.resize(channels);
      for(int i = 0; i < channels; i++)
      {
        out[i].assign(av.data[i], av.data[i] + frames);
      }
    }

    void operator()(const Media::AudioFile::MmapView& av) noexcept
    {
      const int channels = av.wav.channels();
      out.resize(channels);

      // OPTIMIZEME we can just read directly into out[c][i]..
      auto data = std::make_unique<float[]>(frames * channels);
      drwav_read_pcm_frames_f32(av.wav.wav(), frames, data.get());
      for(int i = 0; i < channels; i++)
      {
        out[i].resize(frames);
      }

      for(int64_t i = 0; i < frames; i++)
      {
        for(int c = 0; c < channels; c++)
        {
          out[c][i] = data.get()[i * channels + c];
        }
      }
    }

    void operator()(const Media::AudioFile::StreamView& av) noexcept { }

  } vis{this->decodedSamples(), {}};

  ossia::visit(vis, this->handle());

  return vis.out;
}

std::optional<AudioInfo> probe_drwav(const QFileInfo& fi);
std::optional<AudioInfo> probe(const QString& path)
{
  // FIXME we have to reload everything when the sample rate changes !!
  auto it = AudioDecoder::database().find(path);
  if(it == AudioDecoder::database().end())
  {
    QFileInfo fi{path};
    if(!fi.exists() || !fi.isFile() || !fi.isReadable())
      return std::nullopt;

    const auto& suffix = fi.suffix().toLower();
    if(suffix == "wav" || suffix == "w64")
    {
      if(auto ret = probe_drwav(fi))
      {
        AudioDecoder::database().insert(path, *ret);
        return ret;
      }
    }
    else if(suffix == "aif" || suffix == "aiff")
    {
      if(auto ret = SndfileDecoder::do_probe(path))
      {
        AudioDecoder::database().insert(path, *ret);
        return ret;
      }
    }

    if(auto ret = AudioDecoder::do_probe(path))
    {
      AudioDecoder::database().insert(path, *ret);
      return ret;
    }
    return std::nullopt;
  }
  else
  {
    return *it;
  }
}
}
