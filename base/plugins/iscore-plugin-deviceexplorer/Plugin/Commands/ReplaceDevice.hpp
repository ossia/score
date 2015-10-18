#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "Plugin/Panel/DeviceExplorerModel.hpp"

#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>


namespace DeviceExplorer
{
    namespace Command
    {
        // Replaces all the nodes of a device by new nodes.
        class ReplaceDevice : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL("DeviceExplorerControl", "ReplaceDevice", "ReplaceDevice")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(ReplaceDevice)
                ReplaceDevice(Path<DeviceExplorerModel>&& device_tree,
                              int deviceIndex,
                              iscore::Node&& rootNode);

                void undo() const override;
                void redo() const override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<DeviceExplorerModel> m_deviceTree;
                int m_deviceIndex{};
                iscore::Node m_deviceNode;
                iscore::Node m_savedNode;

        };
    }
}

