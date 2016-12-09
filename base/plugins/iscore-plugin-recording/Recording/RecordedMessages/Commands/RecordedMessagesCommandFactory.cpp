#include "RecordedMessagesCommandFactory.hpp"
#include <iscore/command/Command.hpp>

const CommandParentFactoryKey& RecordedMessages::CommandFactoryName()
{
  static const CommandParentFactoryKey key{"RecordedMessages"};
  return key;
}
