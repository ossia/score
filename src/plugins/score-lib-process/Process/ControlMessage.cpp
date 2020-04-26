#include <Process/ControlMessage.hpp>
#include <Process/Process.hpp>
#include <State/ValueSerialization.hpp>
#include <State/ValueConversion.hpp>
#include <score/model/path/PathSerialization.hpp>

namespace Process
{

QString ControlMessage::name(const score::DocumentContext& ctx) const noexcept
{
  auto port = this->port.try_find(ctx);
  if(port)
  {
    auto parent = qobject_cast<Process::ProcessModel*>(port->parent());
    if(parent)
      return parent->metadata().getName() + " (" + port->customData() + ")";
    else
      return port->customData();
  }
  return QStringLiteral("(deleted)");
}

}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamReader::read(const Process::ControlMessage& mess)
{
  readFrom(mess.port);
  readFrom(mess.value);
  insertDelimiter();
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONReader::read(const Process::ControlMessage& mess)
{
  stream.StartObject();
  obj[strings.Address] = mess.port;
  obj[strings.Value] = mess.value;
  stream.EndObject();
}

template <>
SCORE_LIB_PROCESS_EXPORT void DataStreamWriter::write(Process::ControlMessage& mess)
{
  writeTo(mess.port);
  writeTo(mess.value);

  checkDelimiter();
}

template <>
SCORE_LIB_PROCESS_EXPORT void JSONWriter::write(Process::ControlMessage& mess)
{
  mess.port <<= obj[strings.Address];
  mess.value <<= obj[strings.Value];
}
