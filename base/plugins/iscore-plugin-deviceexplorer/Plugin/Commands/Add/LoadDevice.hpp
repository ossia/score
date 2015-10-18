#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

class DeviceDocumentPlugin;
// Note : could also be used for loading from the library
class LoadDevice: public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL("DeviceExplorerControl", LoadDevice, "LoadDevice")
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
