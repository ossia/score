/*
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
#include <core/presenter/command/CommandQueue.hpp>
#include "tools/NamedObject.hpp"

namespace {
  const QString GeometrySetting("MWGeometry");
}


DeviceExplorerMainWindow::DeviceExplorerMainWindow(QWidget *parent)
	: QWidget(parent)
{
  m_model = new DeviceExplorerModel(this);

  auto cq = qApp->findChild<iscore::CommandQueue*>("CommandQueue");
  Q_ASSERT(cq);
  m_model->setCommandQueue(cq);
  //_proxyModel

  m_treeWidget = new DeviceExplorerWidget(this);
  m_treeWidget->setModel(m_model); //_proxyModel


  QVBoxLayout *layout = new QVBoxLayout;
  layout->addWidget(m_treeWidget);

  this->setLayout(layout);


//  QSettings settings;
//  restoreGeometry(settings.value(GeometrySetting).toByteArray());
}

/*
void
DeviceExplorerMainWindow::closeEvent(QCloseEvent *event)
{
  QSettings settings;
  settings.setValue(GeometrySetting, saveGeometry());
  QWidget::closeEvent(event);
}

void
DeviceExplorerMainWindow::load(const QString &filename)
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
*/
