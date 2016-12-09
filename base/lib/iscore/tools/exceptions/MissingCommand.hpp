#pragma once
#include <iscore/command/Command.hpp>
#include <stdexcept>

namespace iscore
{
/**
 * @brief The MissingCommandException class
 *
 * Is used when a command cannot be instantiated.
 */
class MissingCommandException : public std::runtime_error
{
public:
  MissingCommandException(
      const CommandGroupKey& parent, const CommandKey& command)
      : std::runtime_error{
            ("Could not find " + command.toString() + " in "
             + parent.toString())}
  {
  }
};
}
