#pragma once
#include <iscore/command/Command.hpp>
#define ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(THE_CLASS, ParentName) THE_CLASS () : iscore::SerializableCommand{ ParentName , commandName(), description()} { }
#define ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(THE_CLASS) THE_CLASS () : iscore::SerializableCommand{ factoryName() , commandName(), description() } { }

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
        protected:
            template<typename Str1, typename Str2, typename Str3>
            SerializableCommand(Str1&& parname, Str2&& cmdname, Str3&& text) :
                m_name {cmdname},
                m_parentName {parname},
                m_text{text}
            {
            }


            template<typename T>
            SerializableCommand(const T*) :
                m_name {T::commandName()},
                m_parentName {T::factoryName()},
                m_text{T::description()}
            {
            }

        public:
            ~SerializableCommand();

            const QString& name() const;
            const QString& parentName() const; // TODO rename in factory name ?
            const QString& text() const;
            void setText(const QString& t);

            std::size_t uid() const
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
