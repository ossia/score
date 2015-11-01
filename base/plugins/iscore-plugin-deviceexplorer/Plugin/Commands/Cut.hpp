#pragma once
#include <Plugin/Commands/DeviceExplorerCommandFactory.hpp>

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ModelPath.hpp>

#include <Plugin/Panel/DeviceExplorerModel.hpp>


namespace DeviceExplorer
{
    namespace Command
    {
        class Cut : public iscore::SerializableCommand
        {
                ISCORE_SERIALIZABLE_COMMAND_DECL(DeviceExplorerCommandFactoryName(), Cut, "Cut")
                public:

                Cut(const iscore::NodePath& parentPath,
                    int row,
                        const QString& text,
                        Path<DeviceExplorerModel>&& model);


                void undo() const override;
                void redo() const override;

            protected:
                void serializeImpl(QDataStream&) const override;
                void deserializeImpl(QDataStream&) override;



            protected:
                Path<DeviceExplorerModel> m_model{};
                QByteArray m_data;
                iscore::NodePath m_parentPath;
                int m_row{};
        };

    }
}

