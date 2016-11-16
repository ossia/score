#pragma once
#include <QTableView>
#include <QStandardItemModel>
#include "JSONLibrary.hpp"

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
