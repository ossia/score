// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LibraryWidget.hpp"

#include <QStandardItemModel>
#include <QVBoxLayout>

namespace Library
{
LibraryWidget::LibraryWidget(JSONModel* model, QWidget* parent)
    : QWidget{parent}
{
  auto lay = new QVBoxLayout;
  lay->setMargin(0);
  lay->setContentsMargins(0, 0, 0, 0);

  this->setLayout(lay);

  lay->addWidget(&m_tbl);
  m_tbl.setModel(model);
  m_tbl.setDragEnabled(true);
  m_tbl.setAcceptDrops(true);
  m_tbl.setDropIndicatorShown(true);
}

LibraryWidget::~LibraryWidget()
{
  delete m_tbl.model();
}


ProcessWidget::ProcessWidget(QAbstractItemModel& model, QWidget* parent)
  : QWidget{parent}
{
  auto lay = new QVBoxLayout;
  lay->setMargin(0);
  lay->setContentsMargins(0, 0, 0, 0);

  this->setLayout(lay);

  lay->addWidget(&m_tv);
  m_tv.setModel(&model);
  m_tv.setDragEnabled(true);
  m_tv.setAcceptDrops(true);
  m_tv.setDropIndicatorShown(true);

}

ProcessWidget::~ProcessWidget()
{

}


FileBrowserWidget::FileBrowserWidget(QAbstractItemModel& model, QWidget* parent)
  : QWidget{parent}
{
  auto lay = new QVBoxLayout;
  lay->setMargin(0);
  lay->setContentsMargins(0, 0, 0, 0);

  this->setLayout(lay);

  lay->addWidget(&m_tv);
  m_tv.setModel(&model);
  m_tv.setDragEnabled(true);
  m_tv.setAcceptDrops(true);
  m_tv.setDropIndicatorShown(true);

}

FileBrowserWidget::~FileBrowserWidget()
{

}


SystemLibraryWidget::SystemLibraryWidget(QAbstractItemModel& model, QWidget* parent)
  : QWidget{parent}
{
  auto lay = new QVBoxLayout;
  lay->setMargin(0);
  lay->setContentsMargins(0, 0, 0, 0);

  this->setLayout(lay);

  lay->addWidget(&m_tv);
  m_tv.setModel(&model);
  m_tv.setDragEnabled(true);
  m_tv.setAcceptDrops(true);
  m_tv.setDropIndicatorShown(true);

}

SystemLibraryWidget::~SystemLibraryWidget()
{

}

ProjectLibraryWidget::ProjectLibraryWidget(QAbstractItemModel& model, QWidget* parent)
  : QWidget{parent}
{
  auto lay = new QVBoxLayout;
  lay->setMargin(0);
  lay->setContentsMargins(0, 0, 0, 0);

  this->setLayout(lay);

  lay->addWidget(&m_tv);
  m_tv.setModel(&model);
  m_tv.setDragEnabled(true);
  m_tv.setAcceptDrops(true);
  m_tv.setDropIndicatorShown(true);

}

ProjectLibraryWidget::~ProjectLibraryWidget()
{

}

}
