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
class Remove final : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), Remove, "Remove")
        public:

          Remove(
            Path<DeviceDocumentPlugin> device_tree,
            const iscore::Node& node);

          ~Remove();

        void undo() const override;
        void redo() const override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    protected:
        bool m_device{};

        iscore::SerializableCommand* m_cmd{};

};
}
}
