#pragma once
#include <iscore/command/SerializableCommand.hpp>
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
      const CommandParentFactoryKey& parent, const CommandFactoryKey& command)
      : std::runtime_error{
            ("Could not find " + command.toString() + " in "
             + parent.toString())}
  {
  }
};
}
