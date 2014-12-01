#pragma once

#include <QSortFilterProxyModel>

class DeviceExplorerFilterProxyModel : public QSortFilterProxyModel
{
public:
  DeviceExplorerFilterProxyModel(QObject *parent = nullptr);

  void setColumn(int col);

protected:
  virtual bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

  bool filterAcceptsRowItself(int sourceRow, const QModelIndex &sourceParent) const;
  bool hasAcceptedChildren(int sourceRow, const QModelIndex &sourceParent) const;
  
protected:
  int m_col;

};
