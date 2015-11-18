#include "AddDevice.hpp"
#include <Device/Node/DeviceNode.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

using namespace iscore;
AddDevice::AddDevice(
        Path<DeviceDocumentPlugin>&& device_tree,
        const DeviceSettings& parameters):
    m_devicesModel{device_tree},
    m_parameters(parameters)
{

}

void AddDevice::undo() const
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.removeDevice(m_parameters);
}

void AddDevice::redo() const
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.addDevice(m_parameters);
}

void AddDevice::serializeImpl(DataStreamInput& d) const
{
    d << m_devicesModel;
    d << m_parameters;
}

void AddDevice::deserializeImpl(DataStreamOutput& d)
{
    d >> m_devicesModel;
    d >> m_parameters;
}
