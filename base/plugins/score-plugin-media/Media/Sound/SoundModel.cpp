#include <Media/Sound/SoundModel.hpp>

#include <QFile>
namespace Media
{
namespace Sound
{
ProcessModel::ProcessModel(
        const TimeVal& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
  , outlet{Process::make_outlet(Id<Process::Port>(0), this)}
{
    outlet->setPropagate(true);
    outlet->type = Process::PortType::Audio;
    metadata().setInstanceName(*this);
    setFile("/tmp/bass.aif");
    init();
}

ProcessModel::~ProcessModel()
{

}

void ProcessModel::setFile(const QString &file)
{
    if(file != m_file.name())
    {
        m_file.load(file, score::IDocument::documentContext(*this));
        fileChanged();
    }
}

MediaFileHandle&ProcessModel::file()
{ return m_file; }

const MediaFileHandle&ProcessModel::file() const
{ return m_file; }

int ProcessModel::upmixChannels() const
{
  return m_upmixChannels;
}

int ProcessModel::startChannel() const
{
  return m_startChannel;
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

void ProcessModel::init()
{
  m_outlets.push_back(outlet.get());
  connect(&m_file, &MediaFileHandle::mediaChanged,
          this, [=] {
    if(m_file.channels() == 1) {
      setUpmixChannels(2);
    }
    fileChanged();
  });
}

}

}
