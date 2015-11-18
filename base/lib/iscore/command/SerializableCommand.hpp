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
#define ISCORE_COMMAND_DECL(parentNameFun, name, desc) \
    public: \
        name() = default; \
        virtual const CommandParentFactoryKey& parentKey() const override { return parentNameFun; } \
        virtual const CommandFactoryKey& key() const override { return static_key(); } \
        virtual QString description() const override { return QObject::tr(desc); }  \
    static const CommandFactoryKey& static_key() \
    { \
        static const CommandFactoryKey var{#name}; \
        return var; \
    } \
    private:


class DataStreamInput
{
        QDataStream& m_stream;
    public:
        DataStreamInput(QDataStream& s):
            m_stream{s}
        {

        }

        template<typename T>
        friend DataStreamInput& operator<<(DataStreamInput& s, T&& obj)
        {
            s.m_stream << obj;
            return s;
        }
};

class DataStreamOutput
{
        QDataStream& m_stream;
    public:
        DataStreamOutput(QDataStream& s):
            m_stream{s}
        {

        }

        template<typename T>
        friend DataStreamOutput& operator>>(DataStreamOutput& s, T&& obj)
        {
            s.m_stream >> obj;
            return s;
        }
};


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
        SerializableCommand() = default;
        ~SerializableCommand();

        virtual const CommandParentFactoryKey& parentKey() const = 0;
        virtual const CommandFactoryKey& key() const = 0;
        virtual QString description() const = 0;

        QByteArray serialize() const;
        void deserialize(const QByteArray&);

    protected:
        virtual void serializeImpl(DataStreamInput&) const = 0;
        virtual void deserializeImpl(DataStreamOutput&) = 0;
};
}
