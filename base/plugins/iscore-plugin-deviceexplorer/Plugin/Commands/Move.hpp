#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "Panel/DeviceExplorerModel.hpp"


namespace DeviceExplorer
{
    namespace Command
    {
        class Move : public iscore::SerializableCommand
        {
                ISCORE_SERIALIZABLE_COMMAND_DECL("DeviceExplorerControl", Move, "Move")
                public:

                Move(const iscore::NodePath& srcParentPath, int srcRow, int count,
                         const iscore::NodePath& dstParentPath, int dstRow,
                         const QString& text,
                         Path<DeviceExplorerModel>&& tree_model);


                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(QDataStream&) const override;
                void deserializeImpl(QDataStream&) override;

            protected:
                Path<DeviceExplorerModel> m_model{};
                iscore::NodePath m_srcParentPath;
                iscore::NodePath m_dstParentPath;
                int m_srcRow{};
                int m_dstRow{};
                int m_count{};
        };
    }
}
