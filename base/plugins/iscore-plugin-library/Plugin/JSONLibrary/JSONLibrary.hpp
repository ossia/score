#pragma once
#include <QVector>

#include <QAbstractItemModel>
#include "LibraryElement.hpp"

class JSONModel : public QAbstractItemModel
{
    public:
        JSONModel();

        // Data reading
        QModelIndex index(int row, int column, const QModelIndex &parent) const;
        QModelIndex parent(const QModelIndex &child) const;
        int rowCount(const QModelIndex &parent) const;
        int columnCount(const QModelIndex &parent) const;
        QVariant data(const QModelIndex &index, int role) const;
        QVariant headerData(int section, Qt::Orientation orientation, int role) const;
        Qt::ItemFlags flags(const QModelIndex &index) const;

        // Data removal
        bool removeRows(int row, int count, const QModelIndex &parent);
        bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count, const QModelIndex &destinationParent, int destinationChild);

        // Drag, drop, etc.
        QStringList mimeTypes() const;
        QMimeData *mimeData(const QModelIndexList &indexes) const;
        bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const;
        bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent);
        Qt::DropActions supportedDropActions() const;
        Qt::DropActions supportedDragActions() const;

    private:
        QVector<LibraryElement> elements;
};
