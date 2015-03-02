#pragma once
#include <core/presenter/command/Command.hpp>
#include <QHash>

#define ISCORE_COMMAND_DEFAULT_CTOR(ClassName, ParentName) ClassName(): iscore::SerializableCommand{ParentName, className(), description()} { }
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

            uint32_t uid() const
            {
                return qHash(this->name());
            }

        protected:
            virtual void serializeImpl(QDataStream&) const = 0;
            virtual void deserializeImpl(QDataStream&) = 0;
    };
}
