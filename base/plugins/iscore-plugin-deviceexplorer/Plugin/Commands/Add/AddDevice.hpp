#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>

class DeviceDocumentPlugin;
class AddDevice : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("DeviceExplorerControl", "AddDevice", "AddDevice")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddDevice)
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
