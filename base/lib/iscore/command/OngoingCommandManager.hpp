#pragma once
#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>
#include <core/command/CommandStack.hpp>
#include <iscore/command/AggregateCommand.hpp>

// TODO split this in multiple files.
// Merging strategies
// This is for use with commands that can be merged like Qt's mergeWith() would do.
namespace MergeStrategy
{
    struct Simple
    {
        static void merge(iscore::Command* cmd,
                          iscore::Command* other)
        {
            qDebug() << Q_FUNC_INFO << "TODO";
            Q_ASSERT(false);
            /*
            cmd->mergeWith(other);
            cmd->redo();
            */
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
        static void send(iscore::CommandStack& cmd,
                         iscore::SerializableCommand* other)
        {
            cmd.redoAndPush(other);
        }
    };

    struct Quiet
    {
        static void send(iscore::CommandStack& cmd,
                         iscore::SerializableCommand* other)
        {
            cmd.push(other);
        }
    };
}

namespace CommitStrategy
{
    struct Redo
    {
            static void commit(iscore::CommandStack& stack, iscore::SerializableCommand* cmd)
            {
                SendStrategy::Simple::send(stack, cmd);
            }
    };

    struct UndoRedo
    {
            static void commit(iscore::CommandStack& stack, iscore::SerializableCommand* cmd)
            {
                cmd->undo();
                SendStrategy::Simple::send(stack, cmd);
            }
    };
}


class ICommandDispatcher
{
    public:
        ICommandDispatcher(iscore::CommandStack& stack):
            m_stack{stack}
        {

        }

        iscore::CommandStack& stack() const
        { return m_stack; }

    private:
        iscore::CommandStack& m_stack;
};

template<typename SendStrategy = SendStrategy::Simple>
class CommandDispatcher : public ICommandDispatcher
{
    public:
        template<typename... Args>
        CommandDispatcher(Args&&... args):
            ICommandDispatcher{std::forward<Args&&>(args)...}
        {
        }

        void submitCommand(iscore::SerializableCommand* cmd)
        {
            SendStrategy::send(stack(), cmd);
        }
};


template<typename MergeStrategy_T = MergeStrategy::Simple, typename CommitStrategy_T = CommitStrategy::UndoRedo>
class OngoingCommandDispatcher : public ICommandDispatcher
{
    public:
        template<typename... Args>
        OngoingCommandDispatcher(Args&&... args):
            ICommandDispatcher{std::forward<Args&&>(args)...}
        {
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

        void submitCommand(iscore::SerializableCommand* cmd)
        {
            Q_ASSERT(cmd != nullptr);
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

        void commit()
        {
            if(ongoing())
            {
                CommitStrategy_T::commit(stack(), m_ongoingCommand);
                m_ongoingCommand = nullptr;
                m_ongoing = false;
            }
        }

        void rollback()
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

#include <iscore/locking/ObjectLocker.hpp>
template<typename MergeStrategy_T = MergeStrategy::Simple, typename CommitStrategy_T = CommitStrategy::UndoRedo>
class LockingOngoingCommandDispatcher : public ICommandDispatcher
{
    public:
        template<typename... Args>
        LockingOngoingCommandDispatcher(ObjectPath&& pathToLock, iscore::ObjectLocker& locker, Args&&... args):
                ICommandDispatcher{std::forward<Args&&>(args)...},
            m_locker{std::move(pathToLock), locker}
        {
        }

        const iscore::SerializableCommand* command() const
        {
            return m_ongoingCommand;
        }

        virtual ~LockingOngoingCommandDispatcher()
        {
            delete m_ongoingCommand;
        }

        // True if there is an ongoing command.
        bool ongoing() const
        { return m_ongoing; }

        void submitCommand(iscore::SerializableCommand* cmd)
        {
            Q_ASSERT(cmd != nullptr);
            if(!ongoing())
            {
                m_ongoingCommand = cmd;
                m_ongoingCommand->redo();
                m_ongoing = true;

                m_locker.lock();
            }
            else
            {
                if(m_ongoingCommand->uid() != cmd->uid())
                    qWarning() << "Merging incompatible commands" << m_ongoingCommand->name() << cmd->name();

                MergeStrategy_T::merge(m_ongoingCommand, cmd);
                delete cmd;
            }
        }

        void commit()
        {
            if(ongoing())
            {
                CommitStrategy_T::commit(stack(), m_ongoingCommand);
                m_ongoingCommand = nullptr;
                m_ongoing = false;

                m_locker.unlock();
            }
        }

        void rollback()
        {
            if(ongoing())
            {
                m_ongoing = false;

                m_ongoingCommand->undo();
                delete m_ongoingCommand;
                m_ongoingCommand = nullptr;

                m_locker.unlock();
            }
        }

    protected:
        // TODO unique_ptr
        iscore::SerializableCommand* m_ongoingCommand {};
        bool m_ongoing{};

        iscore::LockHelper m_locker;
};




class MacroCommandDispatcher : public ICommandDispatcher
{
    public:
        template<typename... Args>
        MacroCommandDispatcher(iscore::AggregateCommand* aggregate, Args&&... args):
                ICommandDispatcher{std::forward<Args&&>(args)...},
            m_aggregateCommand{aggregate}
        {
        }


        void submitCommand(iscore::SerializableCommand* cmd)
        {
            m_aggregateCommand->addCommand(cmd);
        }

        void commit()
        {
            if(m_aggregateCommand)
            {
                SendStrategy::Simple::send(stack(), m_aggregateCommand);
                m_aggregateCommand = nullptr;
            }
        }

    protected:
        iscore::AggregateCommand* m_aggregateCommand {};
};




class SingleOngoingCommandDispatcher
{
    public:
        SingleOngoingCommandDispatcher(iscore::CommandStack& stack):
            m_stack{stack}
        {

        }

        ~SingleOngoingCommandDispatcher()
        {
            delete m_cmd;
         }

        iscore::CommandStack& stack() const
        { return m_stack; }

        template<typename TheCommand, typename... Args> // TODO split in two ?
        void submitCommand(Args&&... args)
        {
            if(!m_cmd)
            {
                auto cmd = new TheCommand(std::forward<Args>(args)...);
                cmd->redo();
                m_cmd = cmd;
            }
            else
            {
                Q_ASSERT(m_cmd->uid() == TheCommand::static_uid());
                static_cast<TheCommand*>(m_cmd)->update(std::forward<Args>(args)...);
                static_cast<TheCommand*>(m_cmd)->redo();
            }
        }

        void commit()
        {
            if(m_cmd)
                SendStrategy::Simple::send(stack(), m_cmd);
            m_cmd = nullptr;
        }

        void rollback()
        {
            m_cmd->undo();
            delete m_cmd;
            m_cmd = nullptr;
        }


    private:
        iscore::CommandStack& m_stack;
        iscore::SerializableCommand* m_cmd{};
};
