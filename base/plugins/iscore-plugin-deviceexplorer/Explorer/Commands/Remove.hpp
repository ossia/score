#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class DataStreamInput;
class DataStreamOutput;

namespace DeviceExplorer
{
class DeviceDocumentPlugin;
namespace Command
{
// TODO split this command.
class Remove final :
        public iscore::SerializableCommand
{
          ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), Remove, "Remove an Explorer node")
    public:
        // For addresses
        Remove(Path<DeviceDocumentPlugin> device_tree,
               Device::NodePath&& path);

        // For devices
        Remove(Path<DeviceDocumentPlugin> device_tree,
               const Device::Node& node);

        ~Remove();

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    protected:
        bool m_device{};
        iscore::SerializableCommand* m_cmd{};

};
}
}
