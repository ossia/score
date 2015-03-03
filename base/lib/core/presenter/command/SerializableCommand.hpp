#pragma once
#include <core/presenter/command/Command.hpp>
#include <QHash>
#include <numeric>

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

            int32_t uid() const
            {
                using namespace std;
                auto hash = qHash(this->name());
                int32_t theUid =
                    hash <= numeric_limits<int32_t>::max() ?
                        (int32_t) hash :
                        (int32_t) (hash - numeric_limits<int32_t>::max() - 1) + numeric_limits<int32_t>::min();
                return theUid;
            }

        protected:
            virtual void serializeImpl(QDataStream&) const = 0;
            virtual void deserializeImpl(QDataStream&) = 0;
    };
}
