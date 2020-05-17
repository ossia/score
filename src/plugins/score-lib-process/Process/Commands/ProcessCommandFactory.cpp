#include <Process/Commands/ProcessCommandFactory.hpp>

#include <score/command/Command.hpp>
#include <score/plugins/StringFactoryKey.hpp>
namespace Process
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Process"};
  return key;
}
}
