#pragma once
#include <core/presenter/command/CommandQueue.hpp>

// Merging strategies
// This is for use with commands that can be merged like Qt's mergeWith() would do.
namespace MergeStrategy
{
    struct Simple
    {
        static void merge(iscore::Command* cmd,
                          iscore::Command* other)
        {
            cmd->mergeWith(other);
            cmd->redo();
        }
    };

    // This is for use with commands that should be undone when a new
    // version arrives (like CreateEvent)
    struct Undo
    {
        static void merge(iscore::Command* cmd,
                          iscore::Command* other)
        {
            cmd->undo();
            MergeStrategy::Simple::merge(cmd, other);
        }
    };
}

namespace SendStrategy
{
    struct Simple
    {
        static void send(iscore::CommandStack* cmd,
                         iscore::SerializableCommand* other)
        {
            cmd->push(other);
        }
    };

    struct Quiet
    {
        static void send(iscore::CommandStack* cmd,
                         iscore::SerializableCommand* other)
        {
            cmd->quietPush(other);
        }
    };
}


#include <core/interface/document/DocumentInterface.hpp>
class ICommandDispatcher : public QObject
{
        Q_OBJECT
    public:
        ICommandDispatcher(QObject* parent):
            ICommandDispatcher{iscore::IDocument::commandQueue(iscore::IDocument::documentFromObject(parent)),
                               parent}
        {

        }

        ICommandDispatcher(iscore::CommandStack* queue,
                           QObject* parent):
            QObject{parent},
            m_stack{queue}
        {

        }

        iscore::CommandStack* stack() const
        { return m_stack; }

    signals:
        void submitCommand(iscore::SerializableCommand* cmd);

    private:
        iscore::CommandStack* m_stack{};
};

template<typename SendStrategy = SendStrategy::Simple>
class CommandDispatcher : public ICommandDispatcher
{
    public:
        template<typename... Args>
        CommandDispatcher(Args&&... args):
            ICommandDispatcher{std::forward<Args&&>(args)...}
        {
            connect(this, &CommandDispatcher::submitCommand, this,
                    [&] (iscore::SerializableCommand* cmd) { SendStrategy::send(stack(), cmd); },
                    Qt::DirectConnection);
        }
};

class ITransactionalCommandDispatcher : public ICommandDispatcher
{
        Q_OBJECT
    public:
        using ICommandDispatcher::ICommandDispatcher;

    signals:
        void commit();
        void rollback();
};


template<typename MergeStrategy_T = MergeStrategy::Simple>
class OngoingCommandDispatcher : public ITransactionalCommandDispatcher
{
    public:
        template<typename... Args>
        OngoingCommandDispatcher(Args&&... args):
            ITransactionalCommandDispatcher{std::forward<Args&&>(args)...}
        {
            connect(this, &OngoingCommandDispatcher::submitCommand,
                    this, &OngoingCommandDispatcher::send_impl,
                    Qt::DirectConnection);

            connect(this, &OngoingCommandDispatcher::commit,
                    this, &OngoingCommandDispatcher::commit_impl,
                    Qt::DirectConnection);

            connect(this, &OngoingCommandDispatcher::rollback,
                    this, &OngoingCommandDispatcher::rollback_impl,
                    Qt::DirectConnection);
        }

        const iscore::SerializableCommand* command() const
        {
            return m_ongoingCommand;
        }

        virtual ~OngoingCommandDispatcher()
        {
            delete m_ongoingCommand;
        }

        // True if there is an ongoing command.
        bool ongoing() const
        { return m_ongoing; }

    private:
        void send_impl(iscore::SerializableCommand* cmd)
        {
            // TODO this logic has to be somewhere else, since one
            // could want to merge two different commands.
            if(m_ongoingCommand && cmd->uid() != m_ongoingCommand->uid())
            {
                //rollback_impl();
            }

            if(!ongoing())
            {
                m_ongoingCommand = cmd;
                m_ongoingCommand->redo();
                m_ongoing = true;
            }
            else
            {
                MergeStrategy_T::merge(m_ongoingCommand, cmd);
                delete cmd;
            }
        }

        void commit_impl()
        {
            if(ongoing())
            {
                m_ongoing = false;
                m_ongoingCommand->undo();
                SendStrategy::Simple::send(stack(), m_ongoingCommand);
                m_ongoingCommand = nullptr;
            }
        }

        void rollback_impl()
        {
            if(ongoing())
            {
                m_ongoing = false;

                m_ongoingCommand->undo();
                delete m_ongoingCommand;
                m_ongoingCommand = nullptr;
            }
        }

    protected:
        iscore::SerializableCommand* m_ongoingCommand {};
        bool m_ongoing{};
};


