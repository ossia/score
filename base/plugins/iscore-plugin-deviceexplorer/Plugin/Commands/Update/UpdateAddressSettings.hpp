#pragma once
#include <Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <DeviceExplorer/Address/AddressSettings.hpp>

#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>


class DeviceDocumentPlugin;

namespace DeviceExplorer
{
namespace Command
{
class UpdateAddressSettings : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), UpdateAddressSettings, "UpdateAddressSettings")
        public:
          UpdateAddressSettings(
            Path<DeviceDocumentPlugin>&& device_tree,
            const iscore::NodePath &node,
            const iscore::AddressSettings& parameters);


        void undo() const override;
        void redo() const override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    private:
        Path<DeviceDocumentPlugin> m_devicesModel;

        iscore::NodePath m_node;

        iscore::AddressSettings m_oldParameters;
        iscore::AddressSettings m_newParameters;
};
}
}
