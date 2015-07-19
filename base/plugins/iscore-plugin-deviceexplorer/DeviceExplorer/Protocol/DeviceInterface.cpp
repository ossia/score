#include "DeviceInterface.hpp"


DeviceInterface::DeviceInterface(const iscore::DeviceSettings &s):
    m_settings(s)
{

}

const iscore::DeviceSettings &DeviceInterface::settings() const
{
    return m_settings;
}
