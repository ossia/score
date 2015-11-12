#pragma once
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Device/Node/DeviceNode.hpp>


namespace DeviceExplorer
{
    namespace Command
    {
        // Replaces all the nodes of a device by new nodes.
        class ReplaceDevice final : public iscore::SerializableCommand
        {
            ISCORE_SERIALIZABLE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), ReplaceDevice, "ReplaceDevice")
            public:
                ReplaceDevice(Path<DeviceExplorerModel>&& device_tree,
                              int deviceIndex,
                              iscore::Node&& rootNode);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(QDataStream&) const override;
                void deserializeImpl(QDataStream&) override;

            private:
                Path<DeviceExplorerModel> m_deviceTree;
                int m_deviceIndex{};
                iscore::Node m_deviceNode;
                iscore::Node m_savedNode;

        };
    }
}

