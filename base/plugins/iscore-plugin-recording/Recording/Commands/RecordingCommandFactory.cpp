#include "RecordingCommandFactory.hpp"
#include <iscore/command/Command.hpp>

const CommandParentFactoryKey& RecordingCommandFactoryName()
{
  static const CommandParentFactoryKey key{"Cohesion"};
  return key;
}
