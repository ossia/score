#include <Process/Dataflow/PortSerialization.hpp>

#include <Audio/Settings/Model.hpp>
#include <Media/Sound/SoundModel.hpp>
#include <Media/Tempo.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/tools/File.hpp>

#include <QRegularExpression>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QLatin1StringMatcher>
#endif

#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::Sound::ProcessModel)
namespace Media
{
static const struct default_stretch_modes
{
  default_stretch_modes()
  {
    map["none"] = ossia::audio_stretch_mode::None;
    map["rubberbandstandard"] = ossia::audio_stretch_mode::RubberBandStandard;
    map["rubberbandpercussive"] = ossia::audio_stretch_mode::RubberBandPercussive;
    map["repitch"] = ossia::audio_stretch_mode::Repitch;
    map["rubberbandstandardhq"] = ossia::audio_stretch_mode::RubberBandStandardHQ;
    map["rubberbandpercussivehq"] = ossia::audio_stretch_mode::RubberBandPercussiveHQ;
    map["repitchmediumq"] = ossia::audio_stretch_mode::RepitchMediumQ;
    map["repitchfastestq"] = ossia::audio_stretch_mode::RepitchFastestQ;
    if(auto env = qEnvironmentVariable("SCORE_SOUND_DEFAULT_STRETCH_MODE_UNKNOWN");
       !env.isEmpty())
      unknown = parse(env);
    if(auto env = qEnvironmentVariable("SCORE_SOUND_DEFAULT_STRETCH_MODE_RHYTHM");
       !env.isEmpty())
      rhythm = parse(env);
    if(auto env = qEnvironmentVariable("SCORE_SOUND_DEFAULT_STRETCH_MODE_MELODY");
       !env.isEmpty())
      melody = parse(env);
  }
  ossia::audio_stretch_mode parse(const QString& b)
  {
    auto it = map.find(b);
    if(it != map.end())
      return it->second;
    return ossia::audio_stretch_mode::None;
  }
  boost::container::flat_map<QString, ossia::audio_stretch_mode> map;
  ossia::audio_stretch_mode unknown = ossia::audio_stretch_mode::None;
  ossia::audio_stretch_mode rhythm = ossia::audio_stretch_mode::RubberBandPercussive;
  ossia::audio_stretch_mode melody = ossia::audio_stretch_mode::RubberBandStandard;
} default_stretch_modes;

std::optional<double> estimateTempo(const QString& path)
{
  // we live in a society
  static const QRegularExpression e{"([0-9]+) ?(bpm|BPM|Bpm)"};
  const auto res = e.match(path);
  if(res.hasMatch())
  {
    return res.captured(1).toInt();
  }

  return {};
}

std::optional<double> estimateTempo(const AudioFile& file)
{
  return estimateTempo(file.absoluteFileName());
}

namespace Sound
{
ProcessModel::ProcessModel(
    const TimeVal& duration, const QString& data, const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::
          ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{std::make_unique<Process::AudioOutlet>(
          "Audio Out", Id<Process::Port>(0), this)}
    , m_file{std::make_shared<AudioFile>()}
    , m_mode{default_stretch_modes.unknown}
{
  outlet->setPropagate(true);
  metadata().setInstanceName(*this);
  init();
  setFile(data);

  auto& settings = score::AppContext().settings<Audio::Settings::Model>();
  connect(&settings, &Audio::Settings::Model::RateChanged, this, &ProcessModel::reload);
}

ProcessModel::~ProcessModel() { }

void ProcessModel::loadFile(const QString& file, int stream)
{
  m_file->on_mediaChanged.disconnect<&ProcessModel::on_mediaChanged>(*this);

  m_userFilePath = file;
  auto& ctx = score::IDocument::documentContext(*this);

  m_file
      = AudioFileManager::instance().get(score::locateFilePath(file, ctx), stream, ctx);

  m_file->on_mediaChanged.connect<&ProcessModel::on_mediaChanged>(*this);
}

void ProcessModel::reload()
{
  if(m_file)
  {
    loadFile(m_userFilePath, m_stream);
    on_mediaChanged();
  }
}

void ProcessModel::setFile(const QString& file)
{
  if(file != m_userFilePath)
  {
    m_stream = -1;
    loadFile(file);

    if(auto tempo = m_file->knownTempo())
    {
      setNativeTempo(*tempo);

      auto mode = default_stretch_modes.rhythm;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
      {
        using namespace Qt::StringLiterals;
        static constexpr QLatin1String melodic[] = {
            "pad"_L1,      "key"_L1,      "piano"_L1,    "lead"_L1,        "flute"_L1,
            "guitar"_L1,   "violin"_L1,   "viola"_L1,    "cello"_L1,       "banjo"_L1,
            "sax"_L1,      "clarinet"_L1, "oboe"_L1,     "harpischord"_L1, "clavinet"_L1,
            "string"_L1,   "choir"_L1,    "ensemble"_L1, "vocal"_L1,       "voice"_L1,
            "vox"_L1,      "brass"_L1,    "marimba"_L1,  "bell"_L1,        "bell"_L1,
            "bassline"_L1, "ambiance"_L1, "ambient"_L1};
        QLatin1StringMatcher matcher;
        for(auto& m : melodic)
        {
          if(file.contains(m, Qt::CaseInsensitive))
            mode = default_stretch_modes.melody;
        }
      }
#endif
      setStretchMode(mode);
      setLoops(true);
    }
    else
    {
      setNativeTempo(tempoAtStartDate(*this));
    }
    on_mediaChanged();
    prettyNameChanged();
  }
}

QString ProcessModel::userFilePath() const noexcept
{
  return m_userFilePath;
}

void ProcessModel::setFileForced(const QString& file, int stream)
{
  m_userFilePath = file;
  loadFile(file, stream);

  on_mediaChanged();
  prettyNameChanged();
}

std::shared_ptr<AudioFile>& ProcessModel::file()
{
  return m_file;
}

const std::shared_ptr<AudioFile>& ProcessModel::file() const
{
  return m_file;
}

QString ProcessModel::prettyName() const noexcept
{
  return m_file->empty() ? Process::ProcessModel::prettyName() : m_file->fileName();
}

int ProcessModel::stream() const noexcept
{
  return m_stream;
}

int ProcessModel::upmixChannels() const noexcept
{
  return m_upmixChannels;
}

int ProcessModel::startChannel() const noexcept
{
  return m_startChannel;
}

double ProcessModel::nativeTempo() const noexcept
{
  return m_nativeTempo;
}

ossia::audio_stretch_mode ProcessModel::stretchMode() const noexcept
{
  return m_mode;
}

void ProcessModel::setUpmixChannels(int upmixChannels)
{
  if(m_upmixChannels == upmixChannels)
    return;

  m_upmixChannels = upmixChannels;
  upmixChannelsChanged(m_upmixChannels);
}

void ProcessModel::setStartChannel(int startChannel)
{
  if(m_startChannel == startChannel)
    return;

  m_startChannel = startChannel;
  startChannelChanged(m_startChannel);
}

void ProcessModel::setStream(int stream)
{
  if(m_stream == stream)
    return;

  m_stream = stream;

  reload();
  streamChanged(m_stream);
}

void ProcessModel::setNativeTempo(double t)
{
  if(t != m_nativeTempo)
  {
    m_nativeTempo = t;
    nativeTempoChanged(t);
  }
}

void ProcessModel::setStretchMode(ossia::audio_stretch_mode t)
{
  if(t != m_mode)
  {
    m_mode = t;
    stretchModeChanged(t);
  }
}

void ProcessModel::on_mediaChanged()
{
  auto& audio_settings = score::GUIAppContext().settings<Audio::Settings::Model>();
  if(audio_settings.getAutoStereo() && m_file->channels() == 1)
  {
    setUpmixChannels(2);
  }

  fileChanged();
}

void ProcessModel::init()
{
  m_outlets.push_back(outlet.get());
}

void ProcessModel::ancestorStartDateChanged()
{
  scoreTempoChanged();
}

void ProcessModel::ancestorTempoChanged()
{
  scoreTempoChanged();
}
}
}

template <>
void DataStreamReader::read(const Media::Sound::ProcessModel& proc)
{
  m_stream << proc.m_upmixChannels << proc.m_startChannel << proc.m_mode
           << proc.m_nativeTempo << proc.m_stream << proc.m_userFilePath << *proc.outlet;

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Sound::ProcessModel& proc)
{
  m_stream >> proc.m_upmixChannels >> proc.m_startChannel >> proc.m_mode
      >> proc.m_nativeTempo >> proc.m_stream >> proc.m_userFilePath;

  proc.loadFile(proc.m_userFilePath, proc.m_stream);
  proc.outlet = load_audio_outlet(*this, &proc);

  checkDelimiter();
}

template <>
void JSONReader::read(const Media::Sound::ProcessModel& proc)
{
  obj["File"] = proc.m_userFilePath;
  obj["Outlet"] = *proc.outlet;
  obj["Upmix"] = proc.m_upmixChannels;
  obj["Start"] = proc.m_startChannel;
  obj["Mode"] = (int)proc.m_mode;
  obj["Tempo"] = proc.m_nativeTempo;
  obj["Stream"] = proc.m_stream;
}

template <>
void JSONWriter::write(Media::Sound::ProcessModel& proc)
{
  proc.m_upmixChannels = obj["Upmix"].toInt();
  proc.m_startChannel = obj["Start"].toInt();
  proc.m_mode = (ossia::audio_stretch_mode)obj["Mode"].toInt();
  proc.m_nativeTempo = obj["Tempo"].toDouble();
  if(auto stream = obj.tryGet("Stream"))
  {
    proc.m_stream = stream->toInt();
  }

  proc.loadFile(obj["File"].toString(), proc.m_stream);
  JSONWriter writer{obj["Outlet"]};
  proc.outlet = Process::load_audio_outlet(writer, &proc);
}
