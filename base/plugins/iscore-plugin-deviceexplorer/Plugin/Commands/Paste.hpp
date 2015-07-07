#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include "Panel/DeviceExplorerModel.hpp"
#include "DeviceExplorer/NodePath.hpp"

namespace DeviceExplorer
{
    namespace Command
    {

        class Paste : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("Paste", "Paste")
                public:
                    ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(Paste, "DeviceExplorerControl")

                Paste(const Path& parentPath, int row,
                         const QString& text,
                         ObjectPath&& modelPath);


                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;



            protected:
                ObjectPath m_model{};
                QByteArray m_data;
                Path m_parentPath;
                int m_row{};
        };
    }
}
