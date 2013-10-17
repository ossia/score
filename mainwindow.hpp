#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>

class QGraphicsScene;
class QActionGroup;
class QPointF;

namespace Ui {
  class MainWindow;
}

class MainWindow : public QMainWindow
{
  Q_OBJECT

 // garder quel action des menubar sont checkés
 // Q_PROPERTY(type name READ name WRITE setname NOTIFY nameChanged)
public:
  explicit MainWindow(QWidget *parent = 0);
  ~MainWindow();

public slots:
    void setDirty(bool on=true);
    void setMousePosition(QPoint point);

private slots:
  void updateUi();
  void addItem(QPointF);

private:
  qint16 _addOffset;
  QPoint _previousPoint;
  Ui::MainWindow *ui;
  QGraphicsScene *_scene;
  QActionGroup *m_mouseActionGroup;

  QPoint position();
  void connectItem(QObject *item);
  void createConnections();
  void createActionGroups();

  // QWidget interface
protected:
  virtual void mousePressEvent(QMouseEvent *event);
  virtual void mouseMoveEvent(QMouseEvent *event);


};

#endif // MAINWINDOW_HPP
