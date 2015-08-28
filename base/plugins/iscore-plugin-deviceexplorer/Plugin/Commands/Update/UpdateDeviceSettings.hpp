#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>

class DeviceDocumentPlugin;

namespace DeviceExplorer
{
namespace Command
{
class UpdateDeviceSettings : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("DeviceExplorerControl", "UpdateDeviceSettings", "UpdateDeviceSettings")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(UpdateDeviceSettings)
        UpdateDeviceSettings(
                  Path<DeviceDocumentPlugin>&& device_tree,
                  const QString& name,
                  const iscore::DeviceSettings& parameters);


        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;
        iscore::DeviceSettings m_oldParameters;
        iscore::DeviceSettings m_newParameters;
};
}
}
