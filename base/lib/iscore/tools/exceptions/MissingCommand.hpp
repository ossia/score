#pragma once
#include <stdexcept>
#include <iscore/command/SerializableCommand.hpp>

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
        MissingCommandException(const CommandParentFactoryKey& control, const CommandFactoryKey& command):
            std::runtime_error{("Could not find " + command.toString() + " in " + control.toString())}
        {

        }
};
}
