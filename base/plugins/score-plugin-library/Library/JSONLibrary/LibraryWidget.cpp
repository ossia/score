// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LibraryWidget.hpp"
#include <Library/JSONLibrary/ProcessesItemModel.hpp>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>

namespace Library
{
class InfoWidget final
    : public QWidget
{
public:
  InfoWidget(QWidget* parent)
  {
    auto lay = new QFormLayout{this};
    lay->addRow(tr("Name"), &m_name);
    lay->addRow(tr("Author"), &m_author);
    lay->addRow(tr("Tags"), &m_tags);
  }

  void setData(const ProcessData& d)
  {
    m_name.setText(d.name);
    if(auto f = score::GUIAppContext().interfaces<Process::ProcessFactoryList>().get(d.key))
    {
      m_tags.setText(f->tags().join(", "));
    }
    else
    {
      m_tags.clear();
    }
  }


  QLabel m_name;
  QLabel m_author;
  QLabel m_tags;

};

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
