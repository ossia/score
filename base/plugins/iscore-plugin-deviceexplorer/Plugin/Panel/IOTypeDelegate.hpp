#pragma once

#include <QStyledItemDelegate>

/**
 * @brief The IOTypeDelegate class
 *
 * Delegate for display and editing of the IOType column in the device explorer.
 */
class IOTypeDelegate : public QStyledItemDelegate
{
        Q_OBJECT

    public:
        explicit IOTypeDelegate(QObject* parent = nullptr);

        virtual QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
        virtual void setEditorData(QWidget* editor, const QModelIndex& index) const override;
        virtual void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;

        virtual void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};
