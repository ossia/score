#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>

class DeviceDocumentPlugin;
class AddDevice : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL2("DeviceExplorerControl", "AddDevice", "AddDevice")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(AddDevice)
        AddDevice(Path<DeviceDocumentPlugin>&& device_tree,
                  const iscore::DeviceSettings& parameters);


        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;
        iscore::DeviceSettings m_parameters;
};
