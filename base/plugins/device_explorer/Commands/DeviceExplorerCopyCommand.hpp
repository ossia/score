#pragma once

#include <iscore/command/SerializableCommand.hpp>

#include "Panel/DeviceExplorerModel.hpp"


class DeviceExplorerCopyCommand : public iscore::SerializableCommand
{
    public:

        DeviceExplorerCopyCommand();

        void set(const QModelIndex& parentIndex, int row,
                 const QByteArray& data,
                 const QString& text,
                 DeviceExplorerModel* model);


        virtual void undo() override;
        virtual void redo() override;
        virtual int id() const override;
        virtual bool mergeWith(const Command* other) override;

    protected:
        virtual void serializeImpl(QDataStream&) override;
        virtual void deserializeImpl(QDataStream&) override;



    protected:
        DeviceExplorerModel* m_model;
        QByteArray m_data;
        DeviceExplorerModel::Path m_parentPath;
        int m_row;
};
