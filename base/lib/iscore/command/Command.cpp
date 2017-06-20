#include "Command.hpp"
#include "SettingsCommand.hpp"
#include <iscore/application/ApplicationContext.hpp>
#include <QDataStream>
#include <QIODevice>
#include <QtGlobal>
#include <iscore/command/Command.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
namespace iscore
{
Command::Command() /*:
    m_timestamp{
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())}*/
{
}

Command::~Command() = default;
SettingsCommandBase::~SettingsCommandBase() = default;
/*
quint32 Command::timestamp() const
{
  return static_cast<quint32>(m_timestamp.count());
}

void Command::setTimestamp(quint32 stmp)
{
  m_timestamp = std::chrono::duration<quint32>(stmp);
}*/

QByteArray Command::serialize() const
{
  QByteArray arr;
  {
    QDataStream s(&arr, QIODevice::Append);
    s.setVersion(QDataStream::Qt_5_7);

    //s << timestamp();
    DataStreamInput inp{s};
    serializeImpl(inp);
  }

  return arr;
}

void Command::deserialize(const QByteArray& arr)
{
  QDataStream s(arr);
  s.setVersion(QDataStream::Qt_5_7);

  //quint32 stmp;
  //s >> stmp;

  //setTimestamp(stmp);

  DataStreamOutput outp{s};
  deserializeImpl(outp);
}
}
