#include "IOTypeDelegate.hpp"

#include <QComboBox>

#include <iostream>//DEBUG

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
    //TODO: use an enum inside QVariant !?! or boost::multimap
    comboBox->addItem(QString("<- "), QVariant("<-"));
    comboBox->addItem(QString(" ->"), QVariant("->"));
    comboBox->addItem(QString("<->"), QVariant("<->"));
    comboBox->setFrame(false);

    std::cerr << "IOTypeDelegate::createEditor comboBox: " << comboBox->currentIndex() << "\n";

    return comboBox;
}

void
IOTypeDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    QString value = index.model()->data(index, Qt::EditRole).toString();
    QComboBox* comboBox = static_cast<QComboBox*>(editor);
    const int i = comboBox->findData(value);  //or findText with Qt::MatchContains);

    if(i != -1)
    {
        comboBox->setCurrentIndex(i);
    }

    std::cerr << "IOTypeDelegate::setEditorData comboBox: " << comboBox->currentIndex() << "\n";

}

void
IOTypeDelegate::setModelData(QWidget* editor,
                             QAbstractItemModel* model,
                             const QModelIndex& index) const
{
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
