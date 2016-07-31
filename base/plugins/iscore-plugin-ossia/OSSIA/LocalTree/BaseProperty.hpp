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
        ossia::net::node_base& node;
        ossia::net::address_base& addr;

        BaseProperty(
                ossia::net::node_base& n,
                ossia::net::address_base& a):
            node{n},
            addr{a}
        {

        }

        virtual ~BaseProperty();
};
}
}
