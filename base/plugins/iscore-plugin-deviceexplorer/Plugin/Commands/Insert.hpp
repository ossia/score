#pragma once
/*

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

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
                ISCORE_COMMAND_DECL2("DeviceExplorerControl", "Insert", "Insert")

            public:
                ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR2(Insert)

                Insert(const NodePath& parentPath,
                       int row,
                       iscore::Node&& node,
                       ModelPath<DeviceDocumentPlugin>&& modelPath);

                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;

            protected:
                ModelPath<DeviceDocumentPlugin> m_model;
                iscore::Node m_node;
                NodePath m_parentPath;
                int m_row{};
        };
    }
}
*/
