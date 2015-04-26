#pragma once

#include <iscore/command/SerializableCommand.hpp>

#include "Panel/DeviceExplorerModel.hpp"

namespace DeviceExplorer
{
    namespace Command
    {

        class Move : public iscore::SerializableCommand
        {
            public:

                Move();

                void set(const QModelIndex& srcParentIndex, int srcRow, int count,
                         const QModelIndex& dstParentIndex, int dstRow,
                         const QString& text,
                         DeviceExplorerModel* model);


                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            protected:
                DeviceExplorerModel* m_model{};
                DeviceExplorerModel::Path m_srcParentPath;
                DeviceExplorerModel::Path m_dstParentPath;
                int m_srcRow{};
                int m_dstRow{};
                int m_count{};
        };
    }
}
