#include "DeviceInterface.hpp"


DeviceInterface::DeviceInterface(const DeviceSettings &s):
    m_settings(s)
{

}

const DeviceSettings &DeviceInterface::settings() const
{
    return m_settings;
}
