#ifndef GRAPHICSVIEW_HPP
#define GRAPHICSVIEW_HPP

#include <QGraphicsView>

class GraphicsView : public QGraphicsView
{
  Q_OBJECT
public:
  explicit GraphicsView(QWidget *parent = 0);

signals:
  void mousePosition(QPoint);

public slots:


  // QWidget interface
protected:
  void mousePressEvent(QMouseEvent *);
  void mouseReleaseEvent(QMouseEvent *);
  void mouseMoveEvent(QMouseEvent *);
};

#endif // GRAPHICSVIEW_HPP
