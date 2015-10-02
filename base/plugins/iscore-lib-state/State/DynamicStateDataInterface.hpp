#pragma once
#include <QObject>
#include <State/Message.hpp>
/**
 * @brief The DynamicStateDataInterface class
 *
 * An abstract class that is to be subclassed to provide custom states.
 */
class DynamicStateDataInterface : public QObject
{
        Q_OBJECT
    public:
        explicit DynamicStateDataInterface(QObject* parent):
            QObject{parent}
        {

        }

        virtual QString stateName() const = 0;

        virtual DynamicStateDataInterface* clone(QObject* parent) const = 0;

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
         */
        virtual void setMessages(const iscore::MessageList&)
        {

        }

    signals:
        void stateChanged();
        /**
         * @brief messagesChanged
         * Sent whenever the messages in the process changed.
         *
         */
        void messagesChanged(const iscore::MessageList&);

};
