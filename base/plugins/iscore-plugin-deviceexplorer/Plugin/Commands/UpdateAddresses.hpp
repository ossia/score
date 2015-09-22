#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "Plugin/Panel/DeviceExplorerModel.hpp"

#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>


namespace DeviceExplorer
{
    namespace Command
    {
    // TODO Moveme
        class UpdateAddressesValues : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL_OBSOLETE("UpdateAddressesValues", "UpdateAddressesValues")
            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR_OBSOLETE(UpdateAddressesValues, "DeviceExplorerControl")
                UpdateAddressesValues(Path<DeviceExplorerModel>&& device_tree,
                              const QList<QPair<const iscore::Node*, iscore::Value>>& nodes);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                Path<DeviceExplorerModel> m_deviceTree;

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

