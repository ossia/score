#pragma once
#include <ossia/network/base/address.hpp>
#include <ossia/network/base/node.hpp>
#include <iscore_plugin_ossia_export.h>

namespace Ossia
{
namespace LocalTree
{
class ISCORE_PLUGIN_OSSIA_EXPORT BaseProperty
{
    public:
        OSSIA::net::Node& node;
        OSSIA::net::Address& addr;

        BaseProperty(
                OSSIA::net::Node& n,
                OSSIA::net::Address& a):
            node{n},
            addr{a}
        {

        }

        virtual ~BaseProperty();
};
}
}
