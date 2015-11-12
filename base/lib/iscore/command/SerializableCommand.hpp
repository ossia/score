#pragma once
#include <iscore/command/Command.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>

class CommandTag{};
using CommandFactoryKey = StringKey<CommandTag>;
class CommandParentTag{};
using CommandParentFactoryKey = StringKey<CommandParentTag>;

/**
 * This macro is used to specify the common metadata of commands :
 *  - factory name (e.g. "ScenarioControl")
 *  - command name
 *  - command description
 */
#define ISCORE_SERIALIZABLE_COMMAND_DECL(parentNameFun, name, desc) \
    public: \
        name (): iscore::SerializableCommand{ factoryName() , commandName(), description() } { } \
        static const CommandParentFactoryKey& factoryName() { return parentNameFun; } \
        static CommandFactoryKey commandName() { return CommandFactoryKey{#name}; } \
        static QString description() { return QObject::tr(desc); }  \
    static auto static_uid() \
    { \
        using namespace std; \
        hash<CommandFactoryKey> fn; \
        return fn(commandName()); \
    } \
    private:



namespace iscore
{
/**
 * @brief The SerializableCommand class
 *
 * Adds serialization & deserialization capabilities to Command.
 * Most concrete commands shall inherit from this class.
 */
class SerializableCommand : public Command
{
    public:
        ~SerializableCommand();

        const CommandFactoryKey& name() const;
        const CommandParentFactoryKey& parentName() const; // Note: factoryName() is the constexpr one.
        const QString& text() const;
        void setText(const QString& t);

        SerializableCommand& operator=(const SerializableCommand& other) = default;

        std::size_t uid() const
        {
            std::hash<CommandFactoryKey> fn;
            return fn(this->name());
        }

        QByteArray serialize() const;
        void deserialize(const QByteArray&);

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

        virtual void serializeImpl(QDataStream&) const = 0;
        virtual void deserializeImpl(QDataStream&) = 0;

    private:
        CommandFactoryKey m_name;
        CommandParentFactoryKey m_parentName;
        QString m_text;
};
}
