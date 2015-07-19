#include "IOTypeDelegate.hpp"

#include <QComboBox>

#include <iostream>//DEBUG
#include <DeviceExplorer/Address/IOType.hpp>
IOTypeDelegate::IOTypeDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{

}

QWidget*
IOTypeDelegate::createEditor(QWidget* parent,
                             const QStyleOptionViewItem& /*option*/,
                             const QModelIndex& /*index*/) const
{
    QComboBox* comboBox = new QComboBox(parent);

    comboBox->addItem(iscore::IOTypeStringMap()[iscore::IOType::In], (int)iscore::IOType::In);
    comboBox->addItem(iscore::IOTypeStringMap()[iscore::IOType::Out], (int)iscore::IOType::Out);
    comboBox->addItem(iscore::IOTypeStringMap()[iscore::IOType::InOut], (int)iscore::IOType::InOut);
    comboBox->setFrame(false);

    return comboBox;
}

void
IOTypeDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    QComboBox* comboBox = static_cast<QComboBox*>(editor);
    const int i = comboBox->findData(index.model()->data(index, Qt::EditRole).toInt());

    if(i != -1)
    {
        comboBox->setCurrentIndex(i);
    }
}

void
IOTypeDelegate::setModelData(QWidget* editor,
                             QAbstractItemModel* model,
                             const QModelIndex& index) const
{
    // TODO is this into the undo/redo framework ?
    QComboBox* comboBox = static_cast<QComboBox*>(editor);
    QString value = comboBox->currentText();
    model->setData(index, value, Qt::EditRole);
}

void
IOTypeDelegate::updateEditorGeometry(QWidget* editor,
                                     const QStyleOptionViewItem& option,
                                     const QModelIndex& /*index*/) const
{
    editor->setGeometry(option.rect);
}
