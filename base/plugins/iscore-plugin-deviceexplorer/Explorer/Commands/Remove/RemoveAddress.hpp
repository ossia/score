#pragma once
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Device/Node/DeviceNode.hpp>

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
                   const iscore::NodePath &nodePath);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;
        iscore::NodePath m_nodePath;
        iscore::Node m_savedNode;
};
