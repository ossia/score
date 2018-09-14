#include <Process/Commands/ProcessCommandFactory.hpp>

#include <score/command/Command.hpp>
namespace Process
{
const CommandGroupKey& CommandFactoryName()
{
  static const CommandGroupKey key{"Process"};
  return key;
}
}
