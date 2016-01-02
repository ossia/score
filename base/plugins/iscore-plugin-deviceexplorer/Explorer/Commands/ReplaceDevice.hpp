#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

class DataStreamInput;
class DataStreamOutput;
class DeviceExplorerModel;


namespace DeviceExplorer
{
    namespace Command
    {
        // Replaces all the nodes of a device by new nodes.
        class ReplaceDevice final : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), ReplaceDevice, "Replace a device")
            public:
                ReplaceDevice(Path<DeviceExplorerModel>&& device_tree,
                              int deviceIndex,
                              Device::Node&& rootNode);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

            private:
                Path<DeviceExplorerModel> m_deviceTree;
                int m_deviceIndex{};
                Device::Node m_deviceNode;
                Device::Node m_savedNode;

        };
    }
}

