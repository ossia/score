#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class DataStreamInput;
class DataStreamOutput;
class DeviceDocumentPlugin;

/**
 * @brief The RemoveAddress class
 *
 * Removes an address and its child in the device explorer.
 */
class RemoveAddress final : public iscore::SerializableCommand
{
    ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), RemoveAddress, "Remove an address")
    public:
        RemoveAddress(
                   Path<DeviceDocumentPlugin>&& device_tree,
                   const Device::NodePath &nodePath);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;
        Device::NodePath m_nodePath;
        Device::Node m_savedNode;
};
