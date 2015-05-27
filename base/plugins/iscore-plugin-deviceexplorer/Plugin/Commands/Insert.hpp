#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include "Panel/DeviceExplorerModel.hpp"
#include <DeviceExplorer/Node/Node.hpp>
#include "DeviceExplorer/NodePath.hpp"


namespace DeviceExplorer
{
    namespace Command
    {
        // TODO insert a vector.
        class Insert : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("Insert", "Insert")

            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(Insert, "DeviceExplorerControl")

                Insert(const Path& parentPath,
                       int row,
                       Node&& node,
                       ObjectPath&& modelPath);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            protected:
                ObjectPath m_model;
                Node m_node;
                Path m_parentPath;
                int m_row{};
        };
    }
}
