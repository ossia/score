#ifndef GRAPHICSVIEW_HPP
#define GRAPHICSVIEW_HPP

#include <QGraphicsView>

class GraphicsView : public QGraphicsView
{
  Q_OBJECT
public:
  explicit GraphicsView(QWidget *parent = 0);

signals:
  void mousePosition(QPoint); /// Used to emit mousePosition to the mainWindow's statusBar
  void mousePressAddItem(QPointF);

public slots:
  void mouseDragMode(QAction *); /// The DragMode property holds the behavior for dragging the mouse over the scene while the left mouse button is pressed.

  // QWidget interface
protected:
  void mousePressEvent(QMouseEvent *);
  void mouseReleaseEvent(QMouseEvent *);
  void mouseMoveEvent(QMouseEvent *);
};

#endif // GRAPHICSVIEW_HPP
