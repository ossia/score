#pragma once
#include <QTableView>
#include <QStandardItemModel>
#include "JSONLibrary.hpp"
class LibraryWidget : public QWidget
{
    private:
        QTableView m_tbl;
        QStandardItemModel m_model;
        JSONLibrary* m_library{};

    public:
        LibraryWidget(QWidget* parent);

        void setLibrary(JSONLibrary* lib);
};
