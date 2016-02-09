#pragma once
#include <iscore/command/Command.hpp>
#include <iscore/command/CommandFactoryKey.hpp>
#include <QByteArray>
#include <QString>

class DataStreamInput;
class DataStreamOutput;

namespace iscore
{
/**
 * @brief The SerializableCommand class
 *
 * Adds serialization & deserialization capabilities to Command.
 * Most concrete commands shall inherit from this class.
 */
class ISCORE_LIB_BASE_EXPORT SerializableCommand : public Command
{
    public:
        SerializableCommand() = default;
        ~SerializableCommand();

        QByteArray serialize() const;
        void deserialize(const QByteArray&);

    protected:
        virtual void serializeImpl(DataStreamInput&) const = 0;
        virtual void deserializeImpl(DataStreamOutput&) = 0;
};
}
