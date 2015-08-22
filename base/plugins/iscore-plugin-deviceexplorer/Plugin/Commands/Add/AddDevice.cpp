#include "AddDevice.hpp"
#include <DeviceExplorer/Node/Node.hpp>

#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>

using namespace iscore;
AddDevice::AddDevice(
        ModelPath<DeviceDocumentPlugin>&& device_tree,
        const DeviceSettings& parameters):
    iscore::SerializableCommand(this),
    m_devicesModel{device_tree},
    m_parameters(parameters)
{

}

void AddDevice::undo()
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.removeDevice(m_parameters);
}

void AddDevice::redo()
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.addDevice(m_parameters);
}

void AddDevice::serializeImpl(QDataStream& d) const
{
    d << m_devicesModel;
    d << m_parameters;
}

void AddDevice::deserializeImpl(QDataStream& d)
{
    d >> m_devicesModel;
    d >> m_parameters;
}
