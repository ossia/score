#pragma once
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Node/DeviceNode.hpp>

class DeviceDocumentPlugin;
// Note : could also be used for loading from the library
class LoadDevice final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), LoadDevice, "LoadDevice")
        public:
          LoadDevice(
            Path<DeviceDocumentPlugin>&& device_tree,
            iscore::Node&& node);


        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;
        iscore::Node m_deviceNode;
};
