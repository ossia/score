#pragma once
#include <stdexcept>
#include <QString>
namespace iscore
{
class MissingCommandException : public std::runtime_error
{
    public:
        MissingCommandException(const QString& control, const QString& command):
            std::runtime_error{("Could not find " + command + " in " + control).toStdString()}
        {

        }
};
}
