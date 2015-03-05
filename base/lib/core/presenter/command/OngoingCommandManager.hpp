#pragma once
#include <core/presenter/command/CommandQueue.hpp>

class CommandDispatcher : public QObject
{
    public:
        CommandDispatcher(QObject* parent);
        CommandDispatcher(iscore::CommandStack* queue, QObject* parent):
            QObject{parent},
            m_commandQueue{queue}
        {
        }

        virtual void send(iscore::SerializableCommand* cmd);

    protected:
        iscore::CommandStack* commandQueue() const
        { return m_commandQueue; }

    private:
        iscore::CommandStack* m_commandQueue{};

};

template<typename Merge>
class OngoingCommandDispatcher : public CommandDispatcher
{
    public:
        using CommandDispatcher::CommandDispatcher;
        virtual ~OngoingCommandDispatcher()
        {
            delete m_ongoingCommand;
        }

        // True if there is an ongoing command.
        bool ongoing() const
        { return m_ongoingCommand != nullptr; }

        void send(iscore::SerializableCommand* cmd) override
        {
            // TODO this logic has to be somewhere else, since one
            // could want to merge two different commands.
            //if(m_ongoingCommand && cmd->id() != m_ongoingCommand->uid())
            //{
            //    rollback();
            //}

            if(!m_ongoingCommand)
            {
                m_ongoingCommand = cmd;
                m_ongoingCommand->redo();
            }
            else
            {
                Merge::merge(m_ongoingCommand, cmd);
                delete cmd;
            }
        }

        void commit()
        {
            if(m_ongoingCommand)
            {
                commandQueue()->quietPush(m_ongoingCommand);
                m_ongoingCommand = nullptr;
            }
        }

        void rollback()
        {
            if(m_ongoingCommand)
            {
                m_ongoingCommand->undo();
                delete m_ongoingCommand;
                m_ongoingCommand = nullptr;
            }
        }

    protected:
        iscore::SerializableCommand* m_ongoingCommand {};
};


// Merging strategies
// This is for use with commands that can be merged like Qt's mergeWith() would do.
namespace MergeStrategy
{
    struct Simple
    {
            static void merge(iscore::Command* cmd, iscore::Command* other)
            {
                other->redo();
                cmd->mergeWith(other);
            }
    };

    // This is for use with commands that should be undone when a new
    // version arrives (like CreateEvent)
    struct Undo
    {
            static void merge(iscore::Command* cmd, iscore::Command* other)
            {
                cmd->undo();
                MergeStrategy::Simple::merge(cmd, other);
            }
    };
}
