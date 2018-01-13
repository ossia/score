#include <Media/Input/InputModel.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <QFile>
#include <Process/Dataflow/Port.hpp>

namespace Media
{
namespace Input
{
int ProcessModel::startChannel() const
{
  return m_startChannel;
}

int ProcessModel::numChannel() const
{
  return m_numChannel;
}

void ProcessModel::setStartChannel(int startChannel)
{
  if (m_startChannel == startChannel)
    return;

  m_startChannel = startChannel;
  emit startChannelChanged(m_startChannel);
}

void ProcessModel::setNumChannel(int numChannel)
{
  if (m_numChannel == numChannel)
    return;

  m_numChannel = numChannel;
  emit numChannelChanged(m_numChannel);
}

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
  init();
}

ProcessModel::~ProcessModel()
{

}

}

}

template <>
void DataStreamReader::read(const Media::Input::ProcessModel& proc)
{
  m_stream << *proc.outlet << proc.m_startChannel << proc.m_numChannel;
  insertDelimiter();
}

template <>
void DataStreamWriter::write(Media::Input::ProcessModel& proc)
{
  proc.outlet = make_outlet(*this, &proc);
  m_stream >> proc.m_startChannel >> proc.m_numChannel;
  checkDelimiter();
}

template <>
void JSONObjectReader::read(const Media::Input::ProcessModel& proc)
{
  obj["Outlet"] = toJsonObject(*proc.outlet);
  obj["Start"] = proc.m_startChannel;
  obj["Num"] = proc.m_numChannel;
}

template <>
void JSONObjectWriter::write(Media::Input::ProcessModel& proc)
{
  JSONObjectWriter writer{obj["Outlet"].toObject()};
  proc.outlet = Process::make_outlet(writer, &proc);
  proc.m_startChannel = obj["Start"].toInt();
  proc.m_numChannel = obj["Num"].toInt();
}
