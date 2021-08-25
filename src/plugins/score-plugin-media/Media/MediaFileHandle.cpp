#include "MediaFileHandle.hpp"

#include <Media/AudioDecoder.hpp>
#include <Media/RMSData.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/Bind.hpp>
#include <score/tools/File.hpp>

#include <core/document/Document.hpp>

#include <ossia/detail/apply.hpp>

#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
#include <QRegularExpression>
#endif

#include <Audio/Settings/Model.hpp>

#include <QDebug>
#include <QFileInfo>
#include <QStorageInfo>

#define DR_WAV_NO_STDIO
#include <dr_wav.h>

namespace Media
{

// TODO if it's smaller than e.g. 1 megabyte, it would be worth
// loading it in memory entirely..
// TODO might make sense to do resampling during execution if it's nott too
// expensive?
static DecodingMethod needsDecoding(const QString& path, int rate)
{
  if (path.endsWith("wav", Qt::CaseInsensitive)
      || path.endsWith("w64", Qt::CaseInsensitive))
  {
    const auto& info = probe(path);

    if (info->fileRate == rate)
      return DecodingMethod::Mmap;
    else
      return DecodingMethod::Libav;
  }
  else if(path.endsWith("aiff", Qt::CaseInsensitive)
      || path.endsWith("aif", Qt::CaseInsensitive))
  {
    const auto& info = probe(path);
    if (info->fileRate == rate)
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

void AudioFile::load(const QString& path, const QString& abspath)
{
  m_originalFile = path;
  m_file = abspath;

  const auto& audioSettings
      = score::GUIAppContext().settings<Audio::Settings::Model>();
  const auto rate = audioSettings.getRate();

  switch (needsDecoding(m_file, rate))
  {
    case DecodingMethod::Libav:
      load_ffmpeg(rate);
      break;
    case DecodingMethod::Mmap:
      load_drwav();
      break;
    case DecodingMethod::Sndfile:
      load_sndfile();
      break;
    default:
      break;
  }
}

void AudioFile::load(
    const QString& path,
    const QString& abspath,
    DecodingMethod d)
{
  m_originalFile = path;
  m_file = abspath;

  const auto& audioSettings
      = score::GUIAppContext().settings<Audio::Settings::Model>();
  const auto rate = audioSettings.getRate();

  switch (d)
  {
    case DecodingMethod::Libav:
      load_ffmpeg(rate);
      break;
    case DecodingMethod::Mmap:
      load_drwav();
      break;
    case DecodingMethod::Sndfile:
      load_sndfile();
      break;
    default:
      break;
  }
}

int64_t AudioFile::decodedSamples() const
{
  struct
  {
    int64_t operator()() const noexcept { return 0; }
    int64_t operator()(const libav_ptr& r) const noexcept
    {
      return r->decoder.decoded;
    }
    int64_t operator()(const sndfile_ptr& r) const noexcept
    {
      return r.decoder.decoded;
    }
    int64_t operator()(const mmap_ptr& r) const noexcept
    {
      return r.wav.totalPCMFrameCount();
    }
  } _;
  return ossia::apply(_, m_impl);
}

bool AudioFile::isSupported(const QFile& file)
{
  return file.exists()
         && file.fileName().contains(
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
             QRegularExpression(
                 ".(wav|aif|aiff|flac|ogg|mp3|m4a|w64)",
                 QRegularExpression::CaseInsensitiveOption)
#else
             QRegExp(".(wav|aif|aiff|flac|ogg|mp3|m4a|w64)", Qt::CaseInsensitive)
#endif
         );
}

int64_t AudioFile::samples() const
{
  struct
  {
    int64_t operator()() const noexcept { return 0; }
    int64_t operator()(const libav_ptr& r) const noexcept
    {
      const auto& samples = r->handle->data;
      return samples.size() > 0 ? samples[0].size() : 0;
    }
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
    int64_t operator()() const noexcept { return 0; }
    int64_t operator()(const libav_ptr& r) const noexcept
    {
      return r->handle->data.size();
    }
    int64_t operator()(const sndfile_ptr& r) const noexcept
    {
      return r.handle->data.size();
    }
    int64_t operator()(const mmap_ptr& r) const noexcept
    {
      return r.wav.channels();
    }
  } _;
  return ossia::apply(_, m_impl);
}

const RMSData& AudioFile::rms() const
{
  SCORE_ASSERT(m_rms);
  return *m_rms;
}

void AudioFile::updateSampleRate(int rate)
{
  switch (needsDecoding(m_file, rate))
  {
    case DecodingMethod::Libav:
      load_ffmpeg(rate);
      break;
    case DecodingMethod::Sndfile:
      load_ffmpeg(rate);
      break;
    case DecodingMethod::Mmap:
      load_drwav();
      break;
    default:
      break;
  }
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

template <typename Fun_T, typename T>
struct FrameComputer
{
  int64_t start_frame;
  int64_t end_frame;
  ossia::small_vector<T, 8>& sum;
  Fun_T fun;

  void operator()() const noexcept { }

  void operator()(const AudioFile::RAMView& r) noexcept
  {
    const int channels = r.data.size();
    assert((int)sum.size() == channels);
    if (end_frame - start_frame > 0)
    {
      for (int c = 0; c < channels; c++)
      {
        const auto& vals = r.data[c];
        sum[c] = fun.init(vals[start_frame]);
        for (int64_t i = start_frame + 1; i < end_frame; i++)
          sum[c] = fun(sum[c], (float)vals[i]);
      }
    }
    else if (end_frame == start_frame)
    {
      for (int c = 0; c < channels; c++)
      {
        const auto& vals = r.data[c];
        sum[c] = fun.init(vals[start_frame]);
      }
    }
  }

  void operator()(AudioFile::MmapView& r) noexcept
  {
    auto& wav = r.wav;
    const int channels = wav.channels();
    assert((int)sum.size() == channels);

    if (end_frame - start_frame > 0)
    {
      const int64_t buffer_size = end_frame - start_frame;
      thread_local std::vector<float> data_cache;

      if (Q_UNLIKELY(!wav.seek_to_pcm_frame(start_frame)))
        return;

      float* floats{};
      int num_elems = buffer_size * channels;
      if (num_elems > 10000)
      {
        data_cache.resize(num_elems);
        floats = data_cache.data();
      }
      else
      {
        floats = (float*)alloca(sizeof(float) * num_elems);
      }

      auto max = wav.read_pcm_frames_f32(buffer_size, floats);
      if (Q_UNLIKELY(max == 0))
        return;

      for (int c = 0; c < channels; c++)
      {
        sum[c] = fun.init(floats[c]);
      }

      for (decltype(max) i = 1; i < max; i++)
      {
        for (int c = 0; c < channels; c++)
        {
          const float f = floats[i * channels + c];
          sum[c] = fun(sum[c], f);
        }
      }
    }
    else
    {
      float* val = (float*)alloca(sizeof(float) * channels);
      if (Q_UNLIKELY(!wav.seek_to_pcm_frame(start_frame)))
        return;
      int max = wav.read_pcm_frames_f32(1, val);
      if (Q_UNLIKELY(max == 0))
        return;

      for (int c = 0; c < channels; c++)
      {
        sum[c] = fun.init(val[c]);
      }
    }
  }
};

struct SingleFrameComputer
{
  int64_t start_frame;
  ossia::small_vector<float, 8>& sum;

  void operator()() const noexcept { }

  void operator()(const AudioFile::RAMView& r) noexcept
  {
    const int channels = r.data.size();
    assert((int)sum.size() == channels);
    for (int c = 0; c < channels; c++)
    {
      const auto& vals = r.data[c];
      sum[c] = vals[start_frame];
    }
  }

  void operator()(AudioFile::MmapView& r) noexcept
  {
    auto& wav = r.wav;
    const int channels = wav.channels();
    assert((int)sum.size() == channels);

    float* val = (float*)alloca(sizeof(float) * channels);
    if (Q_UNLIKELY(!wav.seek_to_pcm_frame(start_frame)))
      return;

    int max = wav.read_pcm_frames_f32(1, val);
    if (Q_UNLIKELY(max == 0))
      return;

    for (int c = 0; c < channels; c++)
    {
      sum[c] = val[c];
    }
  }
};

void AudioFile::ViewHandle::frame(
    int64_t start_frame,
    ossia::small_vector<float, 8>& out) noexcept
{
  SingleFrameComputer _{start_frame, out};
  ossia::apply(_, *this);
}

void AudioFile::ViewHandle::absmax_frame(
    int64_t start_frame,
    int64_t end_frame,
    ossia::small_vector<float, 8>& out) noexcept
{
  struct AbsMax
  {
    static float init(float v) noexcept { return v; }
    float operator()(float f1, float f2) const noexcept
    {
      return abs_max(f1, f2);
    }
  };
  FrameComputer<AbsMax, float> _{start_frame, end_frame, out, {}};
  ossia::apply(_, *this);
}

void AudioFile::ViewHandle::minmax_frame(
    int64_t start_frame,
    int64_t end_frame,
    ossia::small_vector<std::pair<float, float>, 8>& out) noexcept
{
  struct MinMax
  {
    static std::pair<float, float> init(float v) noexcept { return {v, v}; }
    auto operator()(std::pair<float, float> f1, float f2) const noexcept
    {
      return std::make_pair(std::min(f1.first, f2), std::max(f1.second, f2));
    }
  };

  FrameComputer<MinMax, std::pair<float, float>> _{
      start_frame, end_frame, out, {}};
  ossia::apply(_, *this);
}

void AudioFile::load_ffmpeg(int rate)
{
  qDebug() << "AudioFileHandle::load_ffmpeg(): " << m_file << rate;
  // Loading with libav is used :
  // - when resampling is required
  // - when the file is not a .wav
  auto ptr = std::make_shared<LibavReader>(rate);
  auto& r = *ptr;
  QFile f{m_file};
  if (isSupported(f))
  {
    r.handle = std::make_shared<ossia::audio_data>();

    auto info = probe(m_file);
    if (!info)
    {
      m_impl = Handle{};
      return;
    }

    m_rms->load(m_file, info->channels, rate, info->duration());

    // TODO remove comment when rms works again if(!m_rms->exists())
    {
      connect(
          &r.decoder,
          &AudioDecoder::newData,
          this,
          [=] {
            const auto& r
                = *eggs::variants::get<std::shared_ptr<LibavReader>>(m_impl);
            std::vector<gsl::span<const audio_sample>> samples;
            auto& handle = r.handle->data;
            const auto decoded = r.decoder.decoded;

            for (auto& channel : handle)
            {
              samples.emplace_back(
                  channel.data(),
                  gsl::span<ossia::audio_sample>::size_type(decoded));
            }
            m_rms->decode(samples);

            on_newData();
          },
          Qt::QueuedConnection);

      connect(
          &r.decoder,
          &AudioDecoder::finishedDecoding,
          this,
          [=] {
            const auto& r
                = *eggs::variants::get<std::shared_ptr<LibavReader>>(m_impl);
            std::vector<gsl::span<const audio_sample>> samples;
            auto& handle = r.handle->data;
            auto decoded = r.decoder.decoded;

            for (auto& channel : handle)
            {
              samples.emplace_back(
                  channel.data(),
                  gsl::span<ossia::audio_sample>::size_type(decoded));
            }
            m_rms->decodeLast(samples);

            m_fullyDecoded = true;
            on_finishedDecoding();
          },
          Qt::QueuedConnection);
    }

    r.decoder.decode(m_file, r.handle);

    m_sampleRate = rate;

    QFileInfo fi{f};

    // Assign pointers to the audio data
    r.data.resize(r.handle->data.size());
    for (std::size_t i = 0; i < r.handle->data.size(); i++)
      r.data[i] = r.handle->data[i].data();

    m_fileName = fi.fileName();
    m_impl = std::move(ptr);
  }
  else
  {
    m_impl = Handle{};
  }
  qDebug() << "AudioFileHandle::on_mediaChanged(): " << m_file;
  on_mediaChanged();
}

void AudioFile::load_drwav()
{
  qDebug() << "AudioFileHandle::load_drwav(): " << m_file;

  // Loading with drwav is done when the file can be
  // mmapped directly in to memory.

  MmapReader r;
  r.file = std::make_shared<QFile>();
  r.file->setFileName(m_file);

  bool ok = r.file->open(QIODevice::ReadOnly);
  if (!ok)
  {
    qDebug() << "Cannot open file" << m_file;
    m_impl = Handle{};
    on_mediaChanged();
  }

  r.data = r.file->map(0, r.file->size());
  if (!r.data)
  {
    qDebug() << "Cannot open file" << m_file;
    m_impl = Handle{};
    on_mediaChanged();
  }
  r.wav.open_memory(r.data, r.file->size());
  if (!r.wav || r.wav.channels() == 0 || r.wav.sampleRate() == 0)
  {
    qDebug() << "Cannot open file" << m_file;
    m_impl = Handle{};
    on_mediaChanged();
  }

  m_rms->load(
      m_file,
      r.wav.channels(),
      r.wav.sampleRate(),
      TimeVal::fromMsecs(
          1000. * r.wav.totalPCMFrameCount() / r.wav.sampleRate()));

  if (!m_rms->exists())
  {
    m_rms->decode(r.wav);
  }

  QFileInfo fi{*r.file};
  m_fileName = fi.fileName();
  m_sampleRate = r.wav.sampleRate();

  m_impl = std::move(r);

  m_fullyDecoded = true;
  on_mediaChanged();
  on_finishedDecoding();
  qDebug() << "AudioFileHandle::on_mediaChanged(): " << m_file;
}

void AudioFile::load_sndfile()
{
  qDebug() << "AudioFileHandle::load_sndfile(): " << m_file;

  // Loading with drwav is done when the file can be
  // mmapped directly in to memory.

  SndfileReader r;

  r.handle = std::make_shared<ossia::audio_data>();
  r.decoder.decode(m_file, r.handle);

  m_rms->load(
      m_file,
      r.decoder.channels,
      r.decoder.fileSampleRate,
      TimeVal::fromMsecs(
          1000. * r.decoder.decoded / r.decoder.fileSampleRate));

  for (auto& channel : r.handle->data)
  {
    r.data.push_back(channel.data());
  }

  m_rms->newData();
  m_rms->finishedDecoding();
  /* FIXME
  if (!m_rms->exists())
  {
    m_rms->decode(r.wav);
  }
  */

  QFileInfo fi{m_file};
  m_fileName = fi.fileName();
  m_sampleRate = r.decoder.fileSampleRate;

  m_impl = std::move(r);

  m_fullyDecoded = true;
  on_mediaChanged();
  on_finishedDecoding();
  qDebug() << "AudioFileHandle::on_mediaChanged(): " << m_file;
}

AudioFileManager::AudioFileManager() noexcept
{
  auto& audioSettings
      = score::GUIAppContext().settings<Audio::Settings::Model>();
  con(audioSettings,
      &Audio::Settings::Model::RateChanged,
      this,
      [this](auto newRate) {
        for (auto& [k, v] : m_handles)
        {
          v->updateSampleRate(newRate);
        }
      });
}

AudioFileManager::~AudioFileManager() noexcept { }

AudioFileManager& AudioFileManager::instance() noexcept
{
  static AudioFileManager m;
  return m;
}

std::shared_ptr<AudioFile>
AudioFileManager::get(const QString& path, const score::DocumentContext& ctx)
{
  // TODO what would be a good garbage collection mechanism ?
  auto abspath = score::locateFilePath(path, ctx);
  if (auto it = m_handles.find(abspath); it != m_handles.end())
  {
    return it->second;
  }
  auto r = std::make_shared<AudioFile>();
  r->load(path, abspath);
  m_handles.insert({abspath, r});
  return r;
}

AudioFile::ViewHandle::ViewHandle(const AudioFile::Handle& handle)
{
  struct
  {
    view_impl_t& self;
    void operator()() const noexcept { }
    void operator()(const libav_ptr& r) const noexcept
    {
      self = RAMView{r->data};
    }
    void operator()(const sndfile_ptr& r) const noexcept
    {
      self = RAMView{r.data};
    }
    void operator()(const mmap_ptr& r) const noexcept
    {
      if (r.wav)
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

  } vis{this->decodedSamples(), {}};

  eggs::variants::apply(vis, this->handle());

  return vis.out;
}

void writeAudioArrayToFile(const QString& path, const ossia::audio_array& arr, int fs)
{
  if(arr.empty())
  {
    qDebug() << "Not writing" << path << ": no data to write.";
    return;
  }

  QFile f{path};
  if(!f.open(QIODevice::WriteOnly))
  {
    qDebug() << "Not writing" << path << ": cannot open file for writing.";
    return;
  }

  const int channels = arr.size();
  const int64_t frames = arr[0].size();
  const int64_t samples = frames * channels;
  const int64_t data_bytes = samples * sizeof(float);
  const int64_t header_bytes = 44;

  // Let's try to not fill the hard drive
  const int64_t minimum_disk_space = 2 * (header_bytes + data_bytes);
  if(QStorageInfo(path).bytesAvailable() < minimum_disk_space)
  {
    qDebug() << "Not writing" << path << ": not enough disk space.";
    return;
  }

  drwav_data_format format;
  drwav wav;

  format.container     = drwav_container_riff;
  format.format        = DR_WAVE_FORMAT_IEEE_FLOAT;
  format.channels      = arr.size();
  format.sampleRate    = fs;
  format.bitsPerSample = 32;

  auto onWrite = [] (void* pUserData, const void* pData, size_t bytesToWrite) -> size_t {
    auto& file = *(QFile*) pUserData;
    return file.write(reinterpret_cast<const char*>(pData), bytesToWrite);
  };

  if(!drwav_init_write_sequential(&wav, &format, samples, onWrite, &f, &ossia::drwav_handle::drwav_allocs))
  {
    qDebug() << "Not writing" << path << ": could not initialize writer.";
    return;
  }

  auto buffer = std::make_unique<float[]>(samples);
  for(int64_t i = 0; i < frames; i++)
  {
    for(int c = 0; c < channels; c++)
    {
      buffer[i * channels + c] = arr[c][i];
    }
  }

  drwav_write_pcm_frames(&wav, frames, buffer.get());
  drwav_uninit(&wav);
  f.flush();
}

std::optional<AudioInfo> probe_drwav(const QFileInfo& fi)
{
  QFile f{fi.absoluteFilePath()};
  // Do a quick pass if it'as a wav file to check for ACID tags
  if (f.open(QIODevice::ReadOnly))
  {
    if (auto data = f.map(0, f.size()))
    {
      ossia::drwav_handle h;
      h.open_memory(data, f.size());

      AudioInfo info;
      info.fileRate = h.sampleRate();
      info.channels = h.channels();
      info.fileLength = h.totalPCMFrameCount();
      info.max_arr_length = h.totalPCMFrameCount();

      if(h.acid().tempo > 0.0001)
        info.tempo = h.acid().tempo;
      else
        info.tempo = estimateTempo(f.fileName());

      return info;
    }
  }

  return std::nullopt;
}

std::optional<AudioInfo> probe(const QString& path)
{
  // FIXME we have to reload everything when the sample rate changes !!
  auto it = AudioDecoder::database().find(path);
  if (it == AudioDecoder::database().end())
  {
    QFileInfo fi{path};
    const auto& suffix = fi.completeSuffix().toLower();
    if (suffix == "wav" || suffix == "w64")
    {
      if(auto ret = probe_drwav(fi))
      {
        AudioDecoder::database().insert(path, *ret);
        return ret;
      }
    }
    else if (suffix == "aif" || suffix == "aiff")
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
