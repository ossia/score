#pragma once
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Device/Protocol/DeviceSettings.hpp>

class DeviceDocumentPlugin;
class AddDevice final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), AddDevice, "AddDevice")
        public:
        AddDevice(Path<DeviceDocumentPlugin>&& device_tree,
                  const iscore::DeviceSettings& parameters);


        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;
        iscore::DeviceSettings m_parameters;
};
