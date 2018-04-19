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
