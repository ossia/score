#pragma once
#include <QVector>

#include <QAbstractItemModel>
#include "LibraryElement.hpp"

class JSONModel : public QAbstractItemModel
{
    public:
        JSONModel();

        // Data reading
        QModelIndex index(int row, int column, const QModelIndex &parent) const override;
        QModelIndex parent(const QModelIndex &child) const override;
        int rowCount(const QModelIndex &parent) const override;
        int columnCount(const QModelIndex &parent) const override;
        QVariant data(const QModelIndex &index, int role) const override;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
        Qt::ItemFlags flags(const QModelIndex &index) const override;

        // Data removal
        bool removeRows(int row, int count, const QModelIndex &parent) override;
        bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild) override;

        // Drag, drop, etc.
        QStringList mimeTypes() const override;
        QMimeData *mimeData(const QModelIndexList &indexes) const override;
        bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const override;
        bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) override;
        Qt::DropActions supportedDropActions() const override;
        Qt::DropActions supportedDragActions() const override;

    private:
        QVector<LibraryElement> elements;
};
