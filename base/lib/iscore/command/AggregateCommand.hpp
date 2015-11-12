#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <QList>
#include <QPair>

/**
 * This macro is used to specify the common metadata of commands :
 *  - factory name (e.g. "ScenarioControl")
 *  - command name
 *  - command description
 */
#define ISCORE_AGGREGATE_COMMAND_DECL(facName, name, desc) \
    public: \
        name (): iscore::AggregateCommand{ factoryName() , commandName(), description() } { } \
        static const CommandParentFactoryKey& factoryName() { return facName; } \
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
    * @brief AggregateCommand: allows for grouping of multiple commands.
    */
    class AggregateCommand : public iscore::SerializableCommand
    {
        public:
            AggregateCommand(
                    const CommandParentFactoryKey& parname,
                    const CommandFactoryKey& cmdname,
                    const QString& text) :
                iscore::SerializableCommand {parname, cmdname, text}
            {
            }

            virtual ~AggregateCommand();

            template<typename T, typename... Args>
            AggregateCommand(
                    const CommandParentFactoryKey& parname,
                    const CommandFactoryKey& cmdname,
                    const QString& text,
                    const T& cmd, Args&& ... remaining) :
                AggregateCommand {parname, cmdname, text, std::forward<Args> (remaining)...}
            {
                m_cmds.push_front(cmd);
            }

            AggregateCommand& operator=(const AggregateCommand& other) = default;

            void undo() const override;
            void redo() const override;

            void addCommand(iscore::SerializableCommand* cmd)
            {
                m_cmds.push_back(cmd);
            }

            int count() const
            { return m_cmds.size(); }

        protected:
            void serializeImpl(QDataStream&) const override;
            void deserializeImpl(QDataStream&) override;

            QList<iscore::SerializableCommand*> m_cmds;
    };
}
