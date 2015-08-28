#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

class DeviceDocumentPlugin;
// Note : could also be used for loading from the library
class LoadDevice: public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL2("DeviceExplorerControl", "LoadDevice", "LoadDevice")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(LoadDevice)
          LoadDevice(
            Path<DeviceDocumentPlugin>&& device_tree,
            iscore::Node&& node);


        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;
        iscore::Node m_deviceNode;
};
