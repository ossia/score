#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <qlist.h>
#include <qpair.h>

#include "State/Value.hpp"

class DataStreamInput;
class DataStreamOutput;
class DeviceExplorerModel;


namespace DeviceExplorer
{
    namespace Command
    {
    // TODO Moveme
        class UpdateAddressesValues final : public iscore::SerializableCommand
        {
            ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), UpdateAddressesValues, "Update addresses values")
            public:
                UpdateAddressesValues(Path<DeviceExplorerModel>&& device_tree,
                              const QList<QPair<const iscore::Node*, iscore::Value>>& nodes);

                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(DataStreamInput&) const override;
                void deserializeImpl(DataStreamOutput&) override;

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

