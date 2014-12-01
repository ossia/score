
#include "MainWindow.hpp"

#include <cassert>
#include <iostream>
#include <QApplication>
#include <QDebug>
#include <QSettings>
#include <QTreeView>
#include <QVBoxLayout>

#include "DeviceExplorerModel.hpp"
#include "DeviceExplorerWidget.hpp"

namespace {
  const QString GeometrySetting("MWGeometry");
}


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{

  QCoreApplication::setOrganizationName("i-score");
  QCoreApplication::setOrganizationDomain("i-score.org");
  QCoreApplication::setApplicationName("DeviceExplorer");
    


  
  m_model = new DeviceExplorerModel(this);

  //_proxyModel

  m_treeWidget = new DeviceExplorerWidget(this);
  m_treeWidget->setModel(m_model); //_proxyModel
  
  
  QVBoxLayout *layout = new QVBoxLayout;
  layout->addWidget(m_treeWidget);

  QWidget *widget = new QWidget(this);
  widget->setLayout(layout);
  setCentralWidget(widget);


  QSettings settings;
  restoreGeometry(settings.value(GeometrySetting).toByteArray());
}

void
MainWindow::closeEvent(QCloseEvent *event)
{
  QSettings settings;
  settings.setValue(GeometrySetting, saveGeometry());
  QMainWindow::closeEvent(event);
}

void
MainWindow::load(const QString &filename)
{
  assert(m_model);

  QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
  
  bool loadOk = m_treeWidget->loadModel(filename);
  if (! loadOk) {
    qDebug()<<"Error: unable to load file: "<<filename<<"\n";
    exit(10);
  }

  QApplication::restoreOverrideCursor();
}
