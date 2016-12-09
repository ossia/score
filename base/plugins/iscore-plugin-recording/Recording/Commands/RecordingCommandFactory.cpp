#include "RecordingCommandFactory.hpp"
#include <iscore/command/Command.hpp>

const CommandGroupKey& RecordingCommandFactoryName()
{
  static const CommandGroupKey key{"Cohesion"};
  return key;
}
