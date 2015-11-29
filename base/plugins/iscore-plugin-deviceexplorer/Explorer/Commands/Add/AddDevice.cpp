#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include "AddDevice.hpp"
#include "Explorer/DocumentPlugin/NodeUpdateProxy.hpp"
#include "iscore/serialization/DataStreamVisitor.hpp"
#include "iscore/tools/ModelPath.hpp"
#include "iscore/tools/ModelPathSerialization.hpp"

namespace iscore {
struct DeviceSettings;
}  // namespace iscore

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
