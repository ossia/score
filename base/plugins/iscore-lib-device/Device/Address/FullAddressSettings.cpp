#include <qstringlist.h>

#include "AddressSettings.hpp"
#include "State/Address.hpp"
#include "State/Message.hpp"
#include "State/Value.hpp"

namespace iscore
{

    // Second argument should be the address of the parent.
    template<>
    FullAddressSettings FullAddressSettings::make<FullAddressSettings::as_parent>(
            const iscore::AddressSettings& other,
            const iscore::Address& addr)
    {
        FullAddressSettings as;
        static_cast<AddressSettingsCommon&>(as) = other;

        as.address = addr;
        as.address.path.append(other.name);

        return as;
    }

    // Second argument should be the address of the resulting FullAddressSettings.
    template<>
    FullAddressSettings FullAddressSettings::make<FullAddressSettings::as_child>(
            const iscore::AddressSettings& other,
            const iscore::Address& addr)
    {
        FullAddressSettings as;
        static_cast<AddressSettingsCommon&>(as) = other;

        as.address = addr;

        return as;
    }

    FullAddressSettings FullAddressSettings::make(
            const iscore::Message& mess)
    {
        FullAddressSettings as;

        as.address = mess.address;
        as.value = mess.value;

        return as;
    }
}
