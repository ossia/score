#pragma once
#include <QObject>
#include <Network/Address.h>
#include <Network/Node.h>

class BaseCallbackWrapper : public QObject
{
    public:
        std::shared_ptr<OSSIA::Node> node;
        std::shared_ptr<OSSIA::Address> addr;

        BaseCallbackWrapper(
                const std::shared_ptr<OSSIA::Node> & n,
                const std::shared_ptr<OSSIA::Address>& a,
                QObject* parent):
            QObject{parent},
            node{n},
            addr{a}
        {

        }

        ~BaseCallbackWrapper()
        {
            addr->removeCallback(m_callbackIt);
        }

    protected:
        OSSIA::Address::iterator m_callbackIt{};
};
