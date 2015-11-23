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
// TODO split this command.
class Remove final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), Remove, "Remove an Explorer node")
        public:

          Remove(
            Path<DeviceDocumentPlugin> device_tree,
            const iscore::Node& node);

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
