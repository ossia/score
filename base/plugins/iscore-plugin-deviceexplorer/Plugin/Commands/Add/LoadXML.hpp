#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <DeviceExplorer/Protocol/DeviceSettings.hpp>
#include <DeviceExplorer/Node/Node.hpp>

class DeviceDocumentPlugin;
// Note : could also be used for loading from the library
class LoadXML: public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL2("DeviceExplorerControl", "LoadXML", "LoadXML")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(LoadXML)
          LoadXML(
            ModelPath<DeviceDocumentPlugin>&& device_tree,
            const iscore::DeviceSettings& parameters,
            const QString& filePath);


        virtual void undo() override;
        virtual void redo() override;

        // After redo(), contains the row of the added device.
        // TODO how to precompute this ?
        int deviceRow() const;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        ModelPath<DeviceDocumentPlugin> m_devicesModel;
        iscore::Node m_deviceNode;
};
