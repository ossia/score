#pragma once
#include <core/presenter/command/CommandQueue.hpp>

class CommandManager : public QObject
{
    public:
        CommandManager(QObject* parent);
        CommandManager(iscore::CommandQueue* queue, QObject* parent):
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

class OngoingCommandManager : public CommandManager
{
        Q_OBJECT
    public:
        using CommandManager::CommandManager;

        ~OngoingCommandManager();

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
