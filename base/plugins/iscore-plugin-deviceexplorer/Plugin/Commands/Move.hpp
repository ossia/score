#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include "Panel/DeviceExplorerModel.hpp"
#include "DeviceExplorer/NodePath.hpp"

namespace DeviceExplorer
{
    namespace Command
    {

        class Move : public iscore::SerializableCommand
        {
            public:

                Move();

                void set(const Path& srcParentPath, int srcRow, int count,
                         const Path& dstParentPath, int dstRow,
                         const QString& text,
                         ObjectPath&& tree_model);


                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            protected:
                ObjectPath m_model{};
                Path m_srcParentPath;
                Path m_dstParentPath;
                int m_srcRow{};
                int m_dstRow{};
                int m_count{};
        };
    }
}
