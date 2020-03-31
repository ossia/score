// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Command.hpp"

#include "SettingsCommand.hpp"

#include <score/application/ApplicationContext.hpp>
#include <score/command/Dispatchers/RuntimeDispatcher.hpp>
#include <score/serialization/DataStreamVisitor.hpp>

namespace score
{
Dispatcher::~Dispatcher() = default;
Command::Command() = default;
Command::~Command() = default;
SettingsCommandBase::~SettingsCommandBase() = default;

QByteArray Command::serialize() const
{
  QByteArray arr;
  {
    QDataStream s(&arr, QIODevice::Append);
    s.setVersion(QDataStream::Qt_5_6);

    DataStreamInput inp{s};
    serializeImpl(inp);
  }

  return arr;
}

void Command::deserialize(const QByteArray& arr)
{
  QDataStream s(arr);
  s.setVersion(QDataStream::Qt_5_6);
  DataStreamOutput outp{s};
  deserializeImpl(outp);
}
}
