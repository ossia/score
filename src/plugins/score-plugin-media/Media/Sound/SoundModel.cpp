#include <Media/Sound/SoundModel.hpp>


#include <wobjectimpl.h>

#include <Audio/Settings/Model.hpp>
W_OBJECT_IMPL(Media::Sound::ProcessModel)
namespace Media
{
namespace Sound
{
ProcessModel::ProcessModel(
    const TimeVal& duration,
    const QString& data,
    const Id<Process::ProcessModel>& id,
    QObject* parent)
    : Process::ProcessModel{duration,
                            id,
                            Metadata<ObjectKey_k, ProcessModel>::get(),
                            parent}
    , outlet{Process::make_outlet(Id<Process::Port>(0), this)}
    , m_file{std::make_shared<AudioFile>()}
{
  outlet->setPropagate(true);
  outlet->type = Process::PortType::Audio;
  metadata().setInstanceName(*this);
  init();
  setFile(data);
}

ProcessModel::~ProcessModel() {}

void ProcessModel::setFile(const QString& file)
{
  if (file != m_file->originalFile())
  {
    m_file->on_mediaChanged.disconnect<&ProcessModel::on_mediaChanged>(*this);

    m_file = AudioFileManager::instance().get(file, score::IDocument::documentContext(*this));

    m_file->on_mediaChanged.connect<&ProcessModel::on_mediaChanged>(*this);
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
  return m_file->empty() ? Process::ProcessModel::prettyName()
                         : m_file->fileName();
}

int ProcessModel::upmixChannels() const
{
  return m_upmixChannels;
}

int ProcessModel::startChannel() const
{
  return m_startChannel;
}

qint32 ProcessModel::startOffset() const
{
  return m_startOffset;
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

void ProcessModel::setStartOffset(qint32 startOffset)
{
  if (m_startOffset == startOffset)
    return;

  m_startOffset = startOffset;
  startOffsetChanged(m_startOffset);
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
           << proc.m_startChannel << proc.m_startOffset << proc.m_endOffset;

  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Sound::ProcessModel& proc)
{
  QString s;
  m_stream >> s;
  proc.setFile(s);
  proc.outlet = make_outlet(*this, &proc);

  m_stream >> proc.m_upmixChannels >> proc.m_startChannel >> proc.m_startOffset
      >> proc.m_endOffset;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Media::Sound::ProcessModel& proc)
{
  obj["File"] = proc.m_file->originalFile();
  obj["Outlet"] = toJsonObject(*proc.outlet);
  obj["Upmix"] = proc.m_upmixChannels;
  obj["Start"] = proc.m_startChannel;
  obj["StartOffset"] = proc.m_startOffset;
  obj["EndOffset"] = proc.m_endOffset;
}

template <>
void JSONObjectWriter::write(Media::Sound::ProcessModel& proc)
{
  proc.setFile(obj["File"].toString());
  JSONObjectWriter writer{obj["Outlet"].toObject()};
  proc.outlet = Process::make_outlet(writer, &proc);
  proc.m_upmixChannels = obj["Upmix"].toInt();
  proc.m_startChannel = obj["Start"].toInt();
  proc.m_startOffset = obj["StartOffset"].toInt();
  proc.m_endOffset = obj["EndOffset"].toInt();
}
