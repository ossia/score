#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include "Panel/DeviceExplorerModel.hpp"


namespace DeviceExplorer
{
    namespace Command
    {
        class Cut : public iscore::SerializableCommand
        {
                ISCORE_COMMAND_DECL("Cut", "Cut")
                public:
                    ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(Cut, "DeviceExplorerControl")

                Cut(const iscore::NodePath& parentPath,
                    int row,
                        const QString& text,
                        ModelPath<DeviceExplorerModel>&& model);


                virtual void undo() override;
                virtual void redo() override;

            protected:
                virtual void serializeImpl(QDataStream&) const override;
                virtual void deserializeImpl(QDataStream&) override;



            protected:
                ModelPath<DeviceExplorerModel> m_model{};
                QByteArray m_data;
                iscore::NodePath m_parentPath;
                int m_row{};
        };

    }
}

