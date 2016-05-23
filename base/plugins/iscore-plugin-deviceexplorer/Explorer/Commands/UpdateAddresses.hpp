#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Commands/DeviceExplorerCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <QList>
#include <QPair>

#include <State/Value.hpp>

struct DataStreamInput;
struct DataStreamOutput;


namespace Explorer
{
class DeviceExplorerModel;
namespace Command
{
// TODO Moveme
class UpdateAddressesValues final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), UpdateAddressesValues, "Update addresses values")
        public:
            UpdateAddressesValues(Path<DeviceExplorerModel>&& device_tree,
                                  const QList<QPair<const Device::Node*, State::Value>>& nodes);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

    private:
        Path<DeviceExplorerModel> m_deviceTree;

        QList<
        QPair<
        Device::NodePath,
        QPair< // First is old, second is new
        State::Value,
        State::Value
        >
        >
        > m_data;
};
}
}

