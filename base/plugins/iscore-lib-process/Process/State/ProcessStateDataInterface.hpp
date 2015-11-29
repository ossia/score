#pragma once
#include <Process/State/MessageNode.hpp>
#include <State/Message.hpp>
#include <boost/optional/optional.hpp>
#include <qstring.h>
#include <vector>

#include "State/Address.hpp"
#include "iscore/tools/IdentifiedObject.hpp"
#include "iscore/tools/SettableIdentifier.hpp"

class Process;
class QObject;

class ProcessStateDataInterface : public IdentifiedObject<ProcessStateDataInterface>
{
        Q_OBJECT
    public:
        ProcessStateDataInterface(Process& model, QObject* parent):
            IdentifiedObject{Id<ProcessStateDataInterface>{}, "", parent},
            m_model{model}
        {
        }

        virtual ~ProcessStateDataInterface();

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
