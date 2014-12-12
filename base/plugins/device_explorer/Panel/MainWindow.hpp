#pragma once

#include <QMainWindow>

class DeviceExplorerModel;
class DeviceExplorerWidget;

class DeviceExplorerMainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit DeviceExplorerMainWindow(QWidget *parent=0);

protected:
  //void closeEvent(QCloseEvent *event);

public slots:
  //void load();
  void load(const QString &filename);

protected:
  virtual void closeEvent(QCloseEvent *event) override;

private:

  DeviceExplorerWidget *m_treeWidget;
  DeviceExplorerModel *m_model;
  
};

