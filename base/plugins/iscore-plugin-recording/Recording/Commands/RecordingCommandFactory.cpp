#include "RecordingCommandFactory.hpp"
#include <iscore/command/SerializableCommand.hpp>

const CommandParentFactoryKey& RecordingCommandFactoryName()
{
  static const CommandParentFactoryKey key{"Cohesion"};
  return key;
}
