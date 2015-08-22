#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include "Panel/DeviceExplorerModel.hpp"
#include <DeviceExplorer/Node/Node.hpp>
#include "DeviceExplorer/NodePath.hpp"

#include "Add/AddAddress.hpp"
#include "Add/AddDevice.hpp"

namespace DeviceExplorer
{
namespace Command
{
class Remove : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL2("DeviceExplorerControl", "Remove", "Remove")
        public:
            ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(Remove)

          Remove(ModelPath<DeviceDocumentPlugin>&& device_tree,
            const iscore::Node& node);
          ~Remove();

        virtual void undo() override;
        virtual void redo() override;

    protected:
        virtual void serializeImpl(QDataStream&) const override;
        virtual void deserializeImpl(QDataStream&) override;

    protected:
        bool m_device{};

        iscore::SerializableCommand* m_cmd;

};
}
}
