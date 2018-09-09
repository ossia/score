#pragma once
#include "JSONLibrary.hpp"

#include <QStandardItemModel>
#include <QTableView>
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

#include <QTreeView>
namespace Library
{
class FileBrowserWidget : public QWidget
{
public:
  FileBrowserWidget(QAbstractItemModel& model, QWidget* parent);
  ~FileBrowserWidget();


private:
  QTreeView m_tv;

};
}

#include <QTreeView>
namespace Library
{
class SystemLibraryWidget : public QWidget
{
public:
  SystemLibraryWidget(QAbstractItemModel& model, QWidget* parent);
  ~SystemLibraryWidget();

  QTreeView& tree() { return m_tv; }

private:
  QTreeView m_tv;

};
}

#include <QTreeView>
namespace Library
{
class ProjectLibraryWidget : public QWidget
{
public:
  ProjectLibraryWidget(QAbstractItemModel& model, QWidget* parent);
  ~ProjectLibraryWidget();

  QTreeView& tree() { return m_tv; }

private:
  QTreeView m_tv;

};
}
