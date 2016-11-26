#pragma once
#include <iscore_lib_device_export.h>
#include <memory>
#include <QMetaType>

namespace ossia
{
namespace net
{
struct domain;
}
}

namespace Device
{
struct ISCORE_LIB_DEVICE_EXPORT Domain
{
        Q_GADGET
    public:
        Domain();
        Domain(const Domain& other);
        Domain(Domain&& other);
        Domain& operator=(const Domain& other);
        Domain& operator=(Domain&& other);
        ~Domain();

        Domain(const ossia::net::domain&);
        Domain& operator=(const ossia::net::domain&);

        operator const ossia::net::domain&() const;
        operator ossia::net::domain&();

        bool operator==(const Device::Domain& other) const;
        bool operator!=(const Device::Domain& other) const;

        const ossia::net::domain& get() const;
        ossia::net::domain& get();

    private:
        std::unique_ptr<ossia::net::domain> domain;
};
}

Q_DECLARE_METATYPE(Device::Domain)
