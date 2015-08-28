#pragma once
#include <stdexcept>
#include <QString>
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
        MissingCommandException(const QString& control, const QString& command):
            std::runtime_error{("Could not find " + command + " in " + control).toStdString()}
        {

        }
};
}
