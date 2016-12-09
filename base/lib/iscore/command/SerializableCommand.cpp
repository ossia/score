#include <QDataStream>
#include <QIODevice>
#include <QtGlobal>
#include <iscore/command/SerializableCommand.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>

namespace iscore
{
SerializableCommand::~SerializableCommand() = default;

QByteArray Command::serialize() const
{
  QByteArray arr;
  {
    QDataStream s(&arr, QIODevice::Append);
    s.setVersion(QDataStream::Qt_5_3);

    s << timestamp();
    DataStreamInput inp{s};
    serializeImpl(inp);
  }

  return arr;
}

void Command::deserialize(const QByteArray& arr)
{
  QDataStream s(arr);
  s.setVersion(QDataStream::Qt_5_3);

  quint32 stmp;
  s >> stmp;

  setTimestamp(stmp);

  DataStreamOutput outp{s};
  deserializeImpl(outp);
}
}
