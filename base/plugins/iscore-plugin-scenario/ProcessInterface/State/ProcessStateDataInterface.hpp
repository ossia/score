#pragma once
#include <QObject>
#include <State/Message.hpp>
#include <ProcessInterface/Process.hpp>
#include <ProcessInterface/State/MessageNode.hpp>

class ProcessStateDataInterface : public QObject
{
    public:
        ProcessStateDataInterface(Process& model, QObject* parent):
            QObject{parent},
            m_model{model}
        {
        }

        Q_OBJECT
    public:
        virtual QString stateName() const = 0;

        virtual ProcessStateDataInterface* clone(QObject* parent) const = 0;

        /**
         * @brief matchingAddresses The addresses that correspond to this state.
         *
         * @return nothing if the process doesn't have any "settable" address.
         * Else it returns the addresses that may change.
         */
        virtual std::vector<iscore::Address> matchingAddresses()
        {
            return {};
        }

        /**
         * @brief messages The current messages in this point of the process.
         */
        virtual iscore::MessageList messages() const
        {
            return {};
        }

        /**
         * @brief setMessages Request a message change on behalf of the process.
         *
         * Should return the actual new state of the process.
         *
         */
        virtual iscore::MessageList setMessages(
                const iscore::MessageList& newMessages,
                const MessageNode& currentState)
        {
            return messages();
        }

        Process& process() const
        { return m_model; }

    signals:
        void stateChanged();
        /**
         * @brief messagesChanged
         * Sent whenever the messages in the process changed.
         *
         */
        void messagesChanged(const iscore::MessageList&);

    private:
        Process& m_model;
};
