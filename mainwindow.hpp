#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QMouseEvent>
#include "graphicstimeevent.hpp"

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

private:
  QPoint position();

  qint16 _addOffset;
  QPoint _previousPoint;
  Ui::MainWindow *ui;
  QGraphicsScene *_scene;

  // QWidget interface
protected:
  virtual void mousePressEvent(QMouseEvent *event);
  virtual void mouseMoveEvent(QMouseEvent *event);
};

#endif // MAINWINDOW_HPP
