#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "Panel/DeviceExplorerModel.hpp"
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

namespace DeviceExplorer
{
namespace Command
{
class Remove : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL("DeviceExplorerControl", Remove, "Remove")
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
