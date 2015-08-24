#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include "Plugin/Panel/DeviceExplorerModel.hpp"

#include <DeviceExplorer/Node/Node.hpp>


namespace DeviceExplorer
{
    namespace Command
    {
        class UpdateAddresses : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL("UpdateAddresses", "UpdateAddresses")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(UpdateAddresses, "DeviceExplorerControl")
                UpdateAddresses(ObjectPath&& device_tree,
                              const QList<QPair<const iscore::Node*, iscore::Value>>& nodes);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_deviceTree;

                QList<
                    QPair<
                        iscore::NodePath,
                        QPair< // First is old, second is new
                            iscore::Value,
                            iscore::Value
                        >
                    >
                > m_data;
        };
    }
}

