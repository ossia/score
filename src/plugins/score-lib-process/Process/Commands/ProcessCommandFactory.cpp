#include <Process/Commands/ProcessCommandFactory.hpp>
#include <score/plugins/StringFactoryKey.hpp>

#include <score/command/Command.hpp>
namespace Process
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Process"};
  return key;
}
}
