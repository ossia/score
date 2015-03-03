#pragma once
#include <core/presenter/command/CommandQueue.hpp>

class OngoingCommandManager : public QObject
{
        Q_OBJECT
    public:
        OngoingCommandManager(iscore::CommandQueue* queue, QObject* parent):
            m_commandQueue{queue}
        {
        }

        ~OngoingCommandManager();

        // True if there is an ongoing command.
        bool ongoing()
        { return m_ongoingCommand != nullptr; }

        void send(iscore::SerializableCommand* cmd);
        void finish();
        void rollback();

    signals:
        void ended();

    private:
        iscore::CommandQueue* m_commandQueue{};
        iscore::SerializableCommand* m_ongoingCommand {};
        //ObjectPath m_lockedObject;
};
