#pragma once

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include "Panel/DeviceExplorerModel.hpp"
#include "DeviceExplorer/NodePath.hpp"


namespace DeviceExplorer
{
    namespace Command
    {

        class Insert : public iscore::SerializableCommand
        {
            public:

                Insert();

                void set(const Path& parentPath, int row,
                         const QByteArray& data,
                         const QString& text,
                         ObjectPath&& modelPath);


                virtual void undo() override;
                virtual void redo() override;
                virtual bool mergeWith(const Command* other) override;

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
