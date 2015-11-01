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
class RemoveAddress : public iscore::SerializableCommand
{
    ISCORE_SERIALIZABLE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), RemoveAddress, "RemoveAddress")
    public:
        RemoveAddress(
                   Path<DeviceDocumentPlugin>&& device_tree,
                   const iscore::NodePath &nodePath);

        void undo() const override;
        void redo() const override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;
        iscore::NodePath m_nodePath;
        iscore::Node m_savedNode;
};
