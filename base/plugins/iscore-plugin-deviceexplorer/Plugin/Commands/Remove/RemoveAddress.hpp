#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "Plugin/Panel/DeviceExplorerModel.hpp"

#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

/**
 * @brief The RemoveAddress class
 *
 * Removes an address and its child in the device explorer.
 */
class RemoveAddress : public iscore::SerializableCommand
{
    ISCORE_COMMAND_DECL("DeviceExplorerControl", "RemoveAddress", "RemoveAddress")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(RemoveAddress)
        RemoveAddress(
                   Path<DeviceDocumentPlugin>&& device_tree,
                   const iscore::NodePath &nodePath);

        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;
        iscore::NodePath m_nodePath;
        iscore::Node m_savedNode;
};
