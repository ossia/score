#include <Media/Sound/SoundModel.hpp>

#include <QRegularExpression>

#include <Audio/Settings/Model.hpp>
#include <wobjectimpl.h>
W_OBJECT_IMPL(Media::Sound::ProcessModel)
namespace Media
{
std::optional<double> estimateTempo(const AudioFile& file)
{
  auto handle = file.unsafe_handle();
  if (auto file = handle.target<AudioFile::mmap_ptr>())
  {
    const auto tempo = file->wav.acid().tempo;
    if (tempo != 0.f)
    {
      return tempo;
    }
  }
  else if (auto file = handle.target<AudioFile::libav_ptr>())
  {
    if ((*file)->tempo != 0.f)
    {
      return (*file)->tempo;
    }
  }

  auto path = file.absoluteFileName();
  static const QRegularExpression e{"([0-9]+) ?(bpm|BPM|Bpm)"};
  const auto res = e.match(path);
  if (res.hasMatch())
  {
    return res.captured(1).toInt();
  }

  return {};
}

namespace Sound
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const QString& data,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
    , outlet{Process::make_audio_outlet(Id<Process::Port>(0), this)}
    , m_file{std::make_shared<AudioFile>()}
{
  outlet->setPropagate(true);
  metadata().setInstanceName(*this);
  init();
  setFile(data);
}

ProcessModel::~ProcessModel() { }

void ProcessModel::setFile(const QString& file)
{
  if (file != m_file->originalFile())
  {
    m_file->on_mediaChanged.disconnect<&ProcessModel::on_mediaChanged>(*this);

    m_file = AudioFileManager::instance().get(file, score::IDocument::documentContext(*this));

    m_file->on_mediaChanged.connect<&ProcessModel::on_mediaChanged>(*this);

    if (auto tempo = estimateTempo(*m_file))
    {
      setNativeTempo(*tempo);
      setStretchMode(ossia::audio_stretch_mode::RubberBandPercussive);
    }
    else
    {
      setNativeTempo(120.); // TODO use the root tempo
    }
    on_mediaChanged();
    prettyNameChanged();
  }
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
  if (m_upmixChannels == upmixChannels)
    return;

  m_upmixChannels = upmixChannels;
  upmixChannelsChanged(m_upmixChannels);
}

void ProcessModel::setStartChannel(int startChannel)
{
  if (m_startChannel == startChannel)
    return;

  m_startChannel = startChannel;
  startChannelChanged(m_startChannel);
}

void ProcessModel::setNativeTempo(double t)
{
  if (t != m_nativeTempo)
  {
    m_nativeTempo = t;
    nativeTempoChanged(t);
  }
}

void ProcessModel::setStretchMode(ossia::audio_stretch_mode t)
{
  if (t != m_mode)
  {
    m_mode = t;
    stretchModeChanged(t);
  }
}

void ProcessModel::on_mediaChanged()
{
  auto& audio_settings = score::GUIAppContext().settings<Audio::Settings::Model>();
  if (audio_settings.getAutoStereo() && m_file->channels() == 1)
  {
    setUpmixChannels(2);
  }

  fileChanged();
}

void ProcessModel::init()
{
  m_outlets.push_back(outlet.get());
}
}
}

template <>
void DataStreamReader::read(const Media::Sound::ProcessModel& proc)
{
  m_stream << proc.m_file->originalFile() << *proc.outlet << proc.m_upmixChannels
           << proc.m_startChannel << proc.m_mode << proc.m_nativeTempo;

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Sound::ProcessModel& proc)
{
  QString s;
  m_stream >> s;
  proc.setFile(s);
  proc.outlet = load_audio_outlet(*this, &proc);

  m_stream >> proc.m_upmixChannels >> proc.m_startChannel >> proc.m_mode >> proc.m_nativeTempo;
  checkDelimiter();
}

template <>
void JSONReader::read(const Media::Sound::ProcessModel& proc)
{
  obj["File"] = proc.m_file->originalFile();
  obj["Outlet"] = *proc.outlet;
  obj["Upmix"] = proc.m_upmixChannels;
  obj["Start"] = proc.m_startChannel;
  obj["Mode"] = (int)proc.m_mode;
  obj["Tempo"] = proc.m_nativeTempo;
}

template <>
void JSONWriter::write(Media::Sound::ProcessModel& proc)
{
  proc.setFile(obj["File"].toString());
  JSONWriter writer{obj["Outlet"]};
  proc.outlet = Process::load_audio_outlet(writer, &proc);
  proc.m_upmixChannels = obj["Upmix"].toInt();
  proc.m_startChannel = obj["Start"].toInt();
  proc.m_mode = (ossia::audio_stretch_mode)obj["Mode"].toInt();
  proc.m_nativeTempo = obj["Tempo"].toDouble();

  if (int off = obj["StartOffset"].toInt(); off != 0)
    proc.m_startOffset = TimeVal::fromMsecs(1000. * off / proc.file()->sampleRate());
}
