#pragma once
#include <core/presenter/command/CommandQueue.hpp>

class CommandDispatcher : public QObject
{
    public:
        CommandDispatcher(QObject* parent);
        CommandDispatcher(iscore::CommandQueue* queue, QObject* parent):
            QObject{parent},
            m_commandQueue{queue}
        {
        }

        virtual void send(iscore::SerializableCommand* cmd);

    protected:
        iscore::CommandQueue* commandQueue() const
        { return m_commandQueue; }

    private:
        iscore::CommandQueue* m_commandQueue{};

};

class OngoingCommandDispatcher : public CommandDispatcher
{
        Q_OBJECT
    public:
        using CommandDispatcher::CommandDispatcher;

        ~OngoingCommandDispatcher();

        // True if there is an ongoing command.
        bool ongoing() const
        { return m_ongoingCommand != nullptr; }

        void send(iscore::SerializableCommand* cmd) override;
        void commit();
        void rollback();

    private:
        iscore::SerializableCommand* m_ongoingCommand {};
        //ObjectPath m_lockedObject;
};
