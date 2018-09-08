#pragma once
#include "JSONLibrary.hpp"

#include <QStandardItemModel>
#include <QTableView>

namespace Library
{
class LibraryWidget : public QWidget
{
private:
  QTableView m_tbl;

public:
  LibraryWidget(JSONModel* model, QWidget* parent);
  ~LibraryWidget();
};

}

#include <QTreeView>
namespace Library
{
class ProcessWidget : public QWidget
{
public:
  ProcessWidget(QAbstractItemModel& model, QWidget* parent);
  ~ProcessWidget();


private:
  QTreeView m_tv;

};
}
