#include "RecordedMessagesCommandFactory.hpp"
#include <iscore/command/Command.hpp>

const CommandGroupKey& RecordedMessages::CommandFactoryName()
{
  static const CommandGroupKey key{"RecordedMessages"};
  return key;
}
