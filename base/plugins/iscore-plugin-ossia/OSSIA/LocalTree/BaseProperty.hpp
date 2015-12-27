#pragma once
#include <Network/Address.h>
#include <Network/Node.h>

class BaseProperty
{
    public:
        std::shared_ptr<OSSIA::Node> node;
        std::shared_ptr<OSSIA::Address> addr;

        BaseProperty(
                const std::shared_ptr<OSSIA::Node> & n,
                const std::shared_ptr<OSSIA::Address>& a):
            node{n},
            addr{a}
        {

        }

        virtual ~BaseProperty();
};
