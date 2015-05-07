#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include "Plugin/Panel/DeviceExplorerModel.hpp"

#include <DeviceExplorer/Node/Node.hpp>
#include "DeviceExplorer/NodePath.hpp"

namespace DeviceExplorer
{
    namespace Command
    {
        // Replaces all the nodes of a device by new nodes.
        class ReplaceDevice : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL("ReplaceDevice", "ReplaceDevice")
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(ReplaceDevice, "DeviceExplorerControl")
                ReplaceDevice(ObjectPath&& device_tree,
                              int deviceIndex,
                              Node rootNode);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_deviceTree;
                int m_deviceIndex{};
                Node m_deviceNode;
                Node m_savedNode;

        };
    }
}

