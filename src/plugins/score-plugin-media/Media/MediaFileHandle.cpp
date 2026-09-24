#include "MediaFileHandle.hpp"

#include <Audio/Settings/Model.hpp>
#include <Media/AudioDecoder.hpp>
#include <Media/Libav.hpp>
#include <Media/RMSData.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <ossia/dataflow/audio_stretch_mode.hpp>
#include <ossia/detail/apply.hpp>
#include <ossia/detail/libav.hpp>
#include <ossia/detail/ssize.hpp>

#include <QDebug>
#include <QFileInfo>
#include <QRegularExpression>
#include <QtGlobal>

#include <optional>
namespace Media
{

// TODO if it's smaller than e.g. 1 megabyte, it would be worth
// loading it in memory entirely..
// TODO might make sense to do resampling during execution if it's not too
// expensive?
static DecodingMethod needsDecoding(const QString& path, int rate)
{
  const auto sz = QFileInfo{path}.size();

#if defined(__EMSCRIPTEN__)
  // wasm/MEMFS policy:
  //  - mmap (drwav) is unavailable/unsafe over MEMFS -> never choose it,
  //  - large media must stream instead of fully decoding into the (2GB-capped)
  //    heap: route >48MB and all video to LibavStream (lazy, on-demand decode),
  //  - small files decode fully to RAM (Libav) for fast random access / waveform.
  {
    constexpr qint64 large_threshold = 48ll * 1024 * 1024;
    if(sz > large_threshold || AudioFile::isSupportedVideo(QFile{path}))
      return DecodingMethod::LibavStream;
    return DecodingMethod::Libav;
  }
#else
  constexpr qint64 large_threshold = 4096ll * 1024 * 1024;

  // Rate mismatches are converted in the graph, so only decodability and RAM
  // cost decide here -- unless the graph has no converter at all.
  const auto rate_ok = [rate](const AudioInfo& info) {
    return ossia::graph_resampling || info.fileRate == rate;
  };

  if(path.endsWith("wav", Qt::CaseInsensitive)
     || path.endsWith("w64", Qt::CaseInsensitive))
  {
    const auto& info = probe(path);

    if(info && (info->flags & AudioInfo::DrwavCanDecode) && rate_ok(*info))
      return DecodingMethod::Mmap;
    else if(sz > large_threshold)
        return DecodingMethod::LibavStream;
    else
      return DecodingMethod::Libav;
  }
  else if(
      path.endsWith("aiff", Qt::CaseInsensitive)
      || path.endsWith("aif", Qt::CaseInsensitive)
      || path.endsWith("aifc", Qt::CaseInsensitive)
      || path.endsWith("caf", Qt::CaseInsensitive))
  {
    const auto& info = probe(path);
    // Sndfile reads the whole file into RAM, so it stays behind the size guard.
    if(info && (info->flags & AudioInfo::SndfileCanDecode) && sz <= large_threshold
       && rate_ok(*info))
      return DecodingMethod::Sndfile;
    else if(sz > large_threshold)
      return DecodingMethod::LibavStream;
    else
      return DecodingMethod::Libav;
  }
  else
  {
    if(AudioFile::isSupportedVideo(QFile{path}))
      return DecodingMethod::LibavStream;
    else if(sz > large_threshold)
      return DecodingMethod::LibavStream;
    else
      return DecodingMethod::Libav;
  }
#endif
}

static std::optional<DecodingMethod> forcedDecodingMethod()
{
  std::optional<DecodingMethod> res;
  const auto e = qEnvironmentVariable("SCORE_AUDIO_DECODING_METHOD").toLower();
  if(e.isEmpty())
    return res;
  if(e == "libav_ram")
    return DecodingMethod::Libav;
  if(e == "libav_stream")
    return DecodingMethod::LibavStream;
  if(e == "mmap")
    return DecodingMethod::Mmap;
  if(e == "sndfile")
    return DecodingMethod::Sndfile;
  return res;
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

  static const auto forced_method = forcedDecodingMethod();
  if(forced_method)
    opt.method = *forced_method;
  else if(m_track != -1)
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
      if(m_track == -1)
      {
        const auto& info = probe(m_file);
        // The drwav and sndfile probes leave -1; those formats hold one stream.
        if(info && info->audioStream >= 0)
        {
          m_track = info->audioStream;
        }
        else
          m_track = 0;
      }
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
    int64_t operator()(const libav_ptr& r) const noexcept
    {
      return r->decoder.decoded.load(std::memory_order_acquire);
    }
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
  constexpr auto rex
      = ".(wav|mp3|m4a|ogg|flac|aif|aiff|aifc|w64|ape|wv|wma|aac|caf|opus|ac3|dts|dtshd)"
        "$";
  return file.exists()
         && file.fileName().contains(
             QRegularExpression(rex, QRegularExpression::CaseInsensitiveOption));
}

bool AudioFile::isSupportedVideo(const QFile& file)
{
  constexpr auto rex = ".(mkv|mov|mp4|h264|avi|hap|mpg|mpeg|imf|mxf|mts|m2ts|mj2|webm|y4m|nut|ts)$";
  return file.exists()
         && file.fileName().contains(
             QRegularExpression(rex, QRegularExpression::CaseInsensitiveOption));
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

std::shared_ptr<const WaveformSummary> AudioFile::waveformSummary() const noexcept
{
  // Only once the file is complete: a table that grew behind the threads
  // walking it would need locking on every read.
  if(!finishedDecoding())
    return {};

  std::lock_guard _{m_summaryMutex};
  if(m_summary)
    return m_summary;
  if(m_summaryUnavailable)
    return {};

  ViewHandle h{m_impl};
  if(!h.supports_summary())
  {
    // What a source can do does not change under us, so this never retries.
    m_summaryUnavailable = true;
    return {};
  }

  const int64_t frames = decodedSamples();
  const int32_t chan = channels();
  if(frames <= 0 || chan <= 0)
    return {};

  auto s = std::make_shared<WaveformSummary>();
  s->channels = chan;
  s->frames = frames;
  s->bucket = WaveformSummary::bucketFor(frames, chan);
  s->bucketCount = (frames + s->bucket - 1) / s->bucket;
  s->data = std::make_unique<FloatPair[]>(std::size_t(s->bucketCount) * chan);

  if(!h.build_summary(*s))
  {
    m_summaryUnavailable = true;
    return {};
  }

  m_summary = std::move(s);
  return m_summary;
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
#if SCORE_HAS_LIBAV
      auto ptr = std::make_shared<ossia::libav_handle>();
      ptr->open(r.path, r.stream, 0);
      if(ptr)
      {
        self = StreamView{std::move(ptr)};
      }
#endif
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
  QString real_path = QFileInfo{path}.canonicalFilePath();
  auto& db = AudioDecoder::database();
  auto it = db.find(real_path);
  if(it == db.end())
  {
    QFileInfo fi{path};
    if(!fi.exists() || !fi.isFile() || !fi.isReadable())
      return std::nullopt;

    const auto& suffix = fi.suffix().toLower();
    if(suffix == "wav" || suffix == "w64")
    {
      if(auto ret = probe_drwav(fi))
      {
        db.insert(path, *ret);
        if(path != real_path)
          db.insert(real_path, *ret);
        return ret;
      }
    }

    if(suffix == "wav" || suffix == "w64" || suffix == "aif" || suffix == "aiff"
       || suffix == "aifc" || suffix == "caf")
    {
      if(auto ret = SndfileDecoder::do_probe(path))
      {
        db.insert(path, *ret);
        if(path != real_path)
          db.insert(real_path, *ret);
        return ret;
      }
    }

    if(auto ret = AudioDecoder::do_probe(path))
    {
      db.insert(path, *ret);
      if(path != real_path)
        db.insert(real_path, *ret);
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
