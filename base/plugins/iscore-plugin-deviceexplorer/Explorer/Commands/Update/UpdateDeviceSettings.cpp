#include "UpdateDeviceSettings.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <boost/range/algorithm/find_if.hpp>

using namespace iscore;
using namespace DeviceExplorer::Command;

UpdateDeviceSettings::UpdateDeviceSettings(
        Path<DeviceDocumentPlugin>&& device_tree,
        const QString &name,
        const DeviceSettings& parameters):
    m_devicesModel{device_tree},
    m_newParameters(parameters)
{
    auto& devplug = m_devicesModel.find();
    auto it = std::find_if(devplug.rootNode().begin(),
                           devplug.rootNode().end(),
                  [&] (const iscore::Node& n)
    { return n.get<DeviceSettings>().name == name; });

    ISCORE_ASSERT(it != devplug.rootNode().end());

    m_oldParameters = (*it).get<DeviceSettings>();
}

void UpdateDeviceSettings::undo() const
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.updateDevice(m_newParameters.name, m_oldParameters);
}

void UpdateDeviceSettings::redo() const
{
    auto& devplug = m_devicesModel.find();
    devplug.updateProxy.updateDevice(m_oldParameters.name, m_newParameters);
}

void UpdateDeviceSettings::serializeImpl(QDataStream& d) const
{
    d << m_devicesModel
      << m_oldParameters
      << m_newParameters;
}

void UpdateDeviceSettings::deserializeImpl(QDataStream& d)
{
    d >> m_devicesModel
      >> m_oldParameters
      >> m_newParameters;
}
