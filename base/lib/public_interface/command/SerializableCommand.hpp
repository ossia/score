#pragma once
#include <public_interface/command/Command.hpp>
#define ISCORE_COMMAND_DEFAULT_CTOR(THE_CLASS, ParentName) THE_CLASS () : iscore::SerializableCommand{ ParentName , className(), description()} { }

namespace iscore
{
    /**
     * @brief The SerializableCommand class
     *
     * Adds serialization & deserialization capabilities to Command.
     *
     */
    class SerializableCommand : public Command
    {
        public:
            using Command::Command;

            QByteArray serialize() const;
            void deserialize(const QByteArray&);

        protected:
            virtual void serializeImpl(QDataStream&) const = 0;
            virtual void deserializeImpl(QDataStream&) = 0;
    };
}
