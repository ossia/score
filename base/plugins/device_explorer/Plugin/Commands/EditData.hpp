#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include "Plugin/Panel/DeviceExplorerModel.hpp"

#include <DeviceExplorer/Node/Node.hpp>

namespace DeviceExplorer
{
    namespace Command
    {
        class EditData : public iscore::SerializableCommand
        {
            ISCORE_COMMAND
            public:
                ISCORE_COMMAND_DEFAULT_CTOR(EditData, "DeviceExplorerControl")
                EditData(ObjectPath&& device_tree,
                            QModelIndex index,
                            QVariant value,
                            int role);

                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            private:
                ObjectPath m_deviceTree;
                DeviceExplorerModel::Path m_nodePath;
                QModelIndex m_index;
                QVariant m_oldValue;
                QVariant m_newValue;
                int m_role;
        };
    }
}

