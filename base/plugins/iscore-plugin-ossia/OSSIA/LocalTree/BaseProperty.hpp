#pragma once
#include <Network/Address.h>
#include <Network/Node.h>
#include <iscore_plugin_ossia_export.h>

namespace Ossia
{
namespace LocalTree
{
class ISCORE_PLUGIN_OSSIA_EXPORT BaseProperty
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
}
}
