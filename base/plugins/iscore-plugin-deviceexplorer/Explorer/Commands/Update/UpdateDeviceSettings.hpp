#pragma once
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

class DeviceDocumentPlugin;

namespace DeviceExplorer
{
namespace Command
{
class UpdateDeviceSettings final : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), UpdateDeviceSettings, "UpdateDeviceSettings")
        public:
        UpdateDeviceSettings(
                  Path<DeviceDocumentPlugin>&& device_tree,
                  const QString& name,
                  const iscore::DeviceSettings& parameters);


        void undo() const override;
        void redo() const override;

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
