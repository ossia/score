#pragma once


#include <QStyledItemDelegate>

class IOTypeDelegate : public QStyledItemDelegate
{
  Q_OBJECT

public:

  IOTypeDelegate(QObject *parent = nullptr);

  //virtual void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override;
  //virtual QSize sizeHint(const QStyleOptionViewItem & option, const QModelIndex & index) const override;


  virtual QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
  virtual void setEditorData(QWidget *editor, const QModelIndex &index) const override;
  virtual void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;

  virtual void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:

  


};
