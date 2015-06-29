#pragma once
#include <iscore/command/Command.hpp>
#define ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(THE_CLASS, ParentName) THE_CLASS () : iscore::SerializableCommand{ ParentName , commandName(), description()} { }

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
            template<typename Str1, typename Str2, typename Str3>
            SerializableCommand(Str1&& parname, Str2&& cmdname, Str3&& text) :
                m_name {cmdname},
                m_parentName {parname},
                m_text{text}
            {
            }

            ~SerializableCommand();

            const QString& name() const;
            const QString& parentName() const;
            const QString& text() const;
            void setText(const QString& t);

            auto uid() const
            {
                std::hash<std::string> fn;
                return fn(this->name().toStdString());
            }

            QByteArray serialize() const;
            void deserialize(const QByteArray&);

        protected:
            virtual void serializeImpl(QDataStream&) const = 0;
            virtual void deserializeImpl(QDataStream&) = 0;

        private:
            const QString m_name;
            const QString m_parentName;
            QString m_text;
    };
}
