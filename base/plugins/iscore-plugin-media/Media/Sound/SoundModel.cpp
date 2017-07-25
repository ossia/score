#include <Media/Sound/SoundModel.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <QFile>
#include <Media/Sound/SoundLayer.hpp>

namespace Media
{
namespace Sound
{

ProcessModel::ProcessModel(
        const TimeVal& duration,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{duration, id, Metadata<ObjectKey_k, ProcessModel>::get(), parent}
{
    metadata().setInstanceName(*this);
    setFile("/tmp/bass.aif");
    init();
}

ProcessModel::ProcessModel(
        const ProcessModel& source,
        const Id<Process::ProcessModel>& id,
        QObject* parent):
    Process::ProcessModel{
        source,
        id,
        Metadata<ObjectKey_k, ProcessModel>::get(),
        parent}
{
    m_file.load(source.m_file.name());
    metadata().setInstanceName(*this);
    init();
}

ProcessModel::~ProcessModel()
{

}

void ProcessModel::setFile(const QString &file)
{
    if(file != m_file.name())
    {
        m_file.load(file);
        emit fileChanged();
    }
}

void ProcessModel::init()
{
  connect(&m_file, &MediaFileHandle::mediaChanged,
          this, &ProcessModel::fileChanged);
}

}

}
