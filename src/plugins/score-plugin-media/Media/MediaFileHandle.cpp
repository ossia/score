#include "MediaFileHandle.hpp"
#include <Media/RMSData.hpp>

#include <Media/AudioDecoder.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/tools/File.hpp>
#include <Audio/Settings/Model.hpp>

#include <score/tools/Bind.hpp>
#include <core/document/Document.hpp>
#include <ossia/detail/apply.hpp>

#include <QDebug>
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
  drwav w{};
  ok = drwav_init_memory(&w, data, file.size(), &ossia::drwav_handle::drwav_allocs);
  if(!ok) {
    return -1;
  }

  auto sr = w.sampleRate;
  drwav_uninit(&w);
  return sr;
}

// TODO if it's smaller than e.g. 1 megabyte, it would be worth
// loading it in memory entirely..
// TODO might make sense to do resampling during execution if it's nott too expensive?
static DecodingMethod needsDecoding(const QString& path, int rate)
{
  if(path.endsWith("wav", Qt::CaseInsensitive) || path.endsWith("w64", Qt::CaseInsensitive))
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

AudioFile::AudioFile()
{
  m_impl = Handle{};
  m_rms = new RMSData{};
}

AudioFile::~AudioFile()
{
  delete m_rms;
}

void AudioFile::load(
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

void AudioFile::load(
    const QString& path,
    const QString& abspath,
    DecodingMethod d)
{
  m_originalFile = path;
  m_file = abspath;

  const auto& audioSettings = score::GUIAppContext().settings<Audio::Settings::Model>();
  const auto rate = audioSettings.getRate();

  switch(d)
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

int64_t AudioFile::decodedSamples() const
{
  struct
  {
  int64_t operator()() const noexcept
  { return 0; }
  int64_t operator()(const libav_ptr& r) const noexcept
  { return r->decoder.decoded; }
  int64_t operator()(const mmap_ptr& r) const noexcept
  { return r.wav.totalPCMFrameCount(); }
  } _;
  return ossia::apply(_, m_impl);
}

bool AudioFile::isSupported(const QFile& file)
{
  return file.exists()
      && file.fileName().contains(
        QRegExp(".(wav|aif|aiff|flac|ogg|mp3|m4a)", Qt::CaseInsensitive));
}

int64_t AudioFile::samples() const
{
  struct
  {
  int64_t operator()() const noexcept
  { return 0; }
  int64_t operator()(const libav_ptr& r) const noexcept
  { const auto& samples = r->handle->data;  return samples.size() > 0 ? samples[0].size() : 0; }
  int64_t operator()(const mmap_ptr& r) const noexcept
  { return r.wav.totalPCMFrameCount(); }
  } _;
  return ossia::apply(_, m_impl);
}

int64_t AudioFile::channels() const
{
  struct
  {
  int64_t operator()() const noexcept
  { return 0; }
  int64_t operator()(const libav_ptr& r) const noexcept
  { return r->handle->data.size(); }
  int64_t operator()(const mmap_ptr& r) const noexcept
  { return r.wav.channels(); }
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

template<typename Fun_T, typename T>
struct FrameComputer
{
  int64_t start_frame;
  int64_t end_frame;
  ossia::small_vector<T, 8> sum;
  Fun_T fun;

  void operator()() const noexcept
  {

  }

  void operator()(const AudioFile::LibavView& r) noexcept
  {
    const int channels = r.data.size();
    sum.resize(channels);
    if(end_frame - start_frame > 0)
    {
      for(int c = 0; c < channels; c++)
      {
        const auto& vals = r.data[c];
        sum[c] = fun.init(vals[start_frame]);
        for(int64_t i = start_frame + 1; i < end_frame; i++)
          sum[c] = fun(sum[c], (float)vals[i]);
        sum[c] = sum[c];
      }
    }
    else if(end_frame == start_frame)
    {
      for(int c = 0; c < channels; c++)
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
    sum.resize(channels);

    if(end_frame - start_frame > 0)
    {
      const int64_t buffer_size = end_frame - start_frame;

      float* floats = (float*)alloca(sizeof(float) * buffer_size * channels);
      if(Q_UNLIKELY(! wav.seek_to_pcm_frame(start_frame)))
        return;

      auto max = wav.read_pcm_frames_f32(buffer_size, floats);
      if(Q_UNLIKELY(max == 0))
        return;

      for(int c = 0; c < channels; c++)
      {
        sum[c] = fun.init(floats[c]);
      }

      for(decltype(max) i = 1; i < max; i++)
      {
        for(int c = 0; c < channels; c++)
        {
          const float f = floats[i * channels + c];
          sum[c] = fun(sum[c], f);
        }
      }
    }
    else
    {
      float* val = (float*)alloca(sizeof(float) * channels);
      if(Q_UNLIKELY(! wav.seek_to_pcm_frame(start_frame)))
        return;
      int max = wav.read_pcm_frames_f32(1, val);
      if(Q_UNLIKELY(max == 0))
        return;

      for(int c = 0; c < channels; c++)
      {
        sum[c] = fun.init(val[c]);
      }
    }
  }
};

struct SingleFrameComputer
{
  int64_t start_frame;
  ossia::small_vector<float, 8> sum;

  void operator()() const noexcept
  {

  }

  void operator()(const AudioFile::LibavView& r) noexcept
  {
    const int channels = r.data.size();
    sum.resize(channels);
    for(int c = 0; c < channels; c++)
    {
      const auto& vals = r.data[c];
      sum[c] = vals[start_frame];
    }
  }

  void operator()(AudioFile::MmapView& r) noexcept
  {
    auto& wav = r.wav;
    const int channels = wav.channels();
    sum.resize(channels);

    float* val = (float*)alloca(sizeof(float) * channels);
    if(Q_UNLIKELY(! wav.seek_to_pcm_frame(start_frame)))
      return;

    int max = wav.read_pcm_frames_f32(1, val);
    if(Q_UNLIKELY(max == 0))
      return;

    for(int c = 0; c < channels; c++)
    {
      sum[c] = val[c];
    }
  }
};

ossia::small_vector<float, 8> AudioFile::ViewHandle::frame(int64_t start_frame) noexcept
{
  SingleFrameComputer _{start_frame, {}};
  ossia::apply(_, *this);
  return _.sum;
}

ossia::small_vector<float, 8> AudioFile::ViewHandle::absmax_frame(int64_t start_frame, int64_t end_frame) noexcept
{
  struct AbsMax
  {
    static float init(float v) noexcept { return v; }
    float operator()(float f1, float f2) const noexcept { return abs_max(f1, f2); }
  };
  FrameComputer<AbsMax, float> _{start_frame, end_frame, {}, {}};
  ossia::apply(_, *this);
  return _.sum;
}


ossia::small_vector<std::pair<float, float>, 8> AudioFile::ViewHandle::minmax_frame(int64_t start_frame, int64_t end_frame) noexcept
{
  struct MinMax
  {
    static std::pair<float, float> init(float v) noexcept { return {v, v}; }
    auto operator()(std::pair<float, float> f1, float f2) const noexcept
    {
      return std::make_pair(std::min(f1.first, f2), std::max(f1.second, f2));
    }
  };

  FrameComputer<MinMax, std::pair<float, float>> _{start_frame, end_frame, {}, {}};
  ossia::apply(_, *this);
  return _.sum;
}

void AudioFile::load_ffmpeg(int rate)
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
      m_impl = Handle{};
      return;
    }

    m_rms->load(m_file, info->channels, rate, info->duration());

    // TODO remove comment when rms works again if(!m_rms->exists())
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

    QFileInfo fi{f};
    if(fi.completeSuffix() == "wav")
    {
      // Do a quick pass if it'as a wav file to check for ACID tags
      if(f.open(QIODevice::ReadOnly)) {
        if(auto data = f.map(0, f.size()))
        {
          ossia::drwav_handle h;
          h.open_memory(data, f.size());

          ptr->tempo = h.acid().tempo;
        }
      }
    }

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
  if(!ok) {
    m_impl = Handle{};
    on_mediaChanged();
  }

  r.data = r.file->map(0, r.file->size());
  if(!r.data) {
    m_impl = Handle{};
    on_mediaChanged();
  }
  r.wav.open_memory(r.data, r.file->size());
  if(!r.wav) {
    m_impl = Handle{};
    on_mediaChanged();
  }

  m_rms->load(m_file,
              r.wav.channels(),
              r.wav.sampleRate(),
              TimeVal::fromMsecs(1000. * r.wav.totalPCMFrameCount() / r.wav.sampleRate())
  );

  if(!m_rms->exists())
  {
    m_rms->decode(r.wav);
  }

  QFileInfo fi{*r.file};
  m_fileName = fi.fileName();
  m_sampleRate = r.wav.sampleRate();

  m_impl = std::move(r);


  on_finishedDecoding();
  on_mediaChanged();
  qDebug() << "AudioFileHandle::on_mediaChanged(): " << m_file;
}

AudioFileManager::AudioFileManager() noexcept
{
  auto& audioSettings = score::GUIAppContext().settings<Audio::Settings::Model>();
  con(audioSettings, &Audio::Settings::Model::RateChanged,
      this, [this] (auto newRate) {
    for(auto& [k, v] : m_handles) {
      v->updateSampleRate(newRate);
    }
  });
}

AudioFileManager::~AudioFileManager() noexcept
{

}

AudioFileManager&AudioFileManager::instance() noexcept
{
  static AudioFileManager m;
  return m;
}

std::shared_ptr<AudioFile> AudioFileManager::get(
    const QString& path,
    const score::DocumentContext& ctx)
{
  // TODO what would be a good garbage collection mechanism ?
  auto abspath = score::locateFilePath(path, ctx);
  if(auto it = m_handles.find(abspath); it != m_handles.end())
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
  void operator()() const noexcept
  {  }
  void operator()(const libav_ptr& r) const noexcept
  { self = LibavView{r->data}; }
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

}
