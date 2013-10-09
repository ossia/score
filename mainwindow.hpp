#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QMouseEvent>
#include "graphicstimeevent.hpp"
#include "graphicstimeprocess.hpp"

class QGraphicsScene;

namespace Ui {
  class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

public slots:
    void setDirty(bool on=true);
    void setMousePosition(QPoint point);

private slots:
  void updateUi();
  void addItem();

private:
  qint16 _addOffset;
  QPoint _previousPoint;
  Ui::MainWindow *ui;
  QGraphicsScene *_scene;

  QPoint position();
  void connectItem(QObject *item);

  // QWidget interface
protected:
  virtual void mousePressEvent(QMouseEvent *event);
  virtual void mouseMoveEvent(QMouseEvent *event);


};

#endif // MAINWINDOW_HPP
