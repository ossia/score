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
SCORE_LIB_STATE_EXPORT void DataStreamReader::read(const Process::ControlMessage& mess)
{
  readFrom(mess.port);
  readFrom(mess.value);
  insertDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONObjectReader::read(const Process::ControlMessage& mess)
{
  obj[strings.Address] = toJsonObject(mess.port);
  obj[strings.Type] = State::convert::textualType(mess.value);
  obj[strings.Value] = ValueToJson(mess.value);
}

template <>
SCORE_LIB_STATE_EXPORT void DataStreamWriter::write(Process::ControlMessage& mess)
{
  writeTo(mess.port);
  writeTo(mess.value);

  checkDelimiter();
}

template <>
SCORE_LIB_STATE_EXPORT void JSONObjectWriter::write(Process::ControlMessage& mess)
{
  mess.port = fromJsonObject<Path<Process::Inlet>>(obj[strings.Address]);
  mess.value = State::convert::fromQJsonValue(
      obj[strings.Value], obj[strings.Type].toString());
}
