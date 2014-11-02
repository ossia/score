/*
Copyright: LaBRI / SCRIME

This software is governed by the CeCILL license under French law and
abiding by the rules of distribution of free software.  You can  use,
modify and/ or redistribute the software under the terms of the CeCILL
license as circulated by CEA, CNRS and INRIA at the following URL
"http://www.cecill.info".

As a counterpart to the access to the source code and  rights to copy,
modify and redistribute granted by the license, users are provided only
with a limited warranty  and the software's author,  the holder of the
economic rights,  and the successive licensors  have only  limited
liability.

In this respect, the user's attention is drawn to the risks associated
with loading,  using,  modifying and/or developing or reproducing the
software by the user in light of its specific status of free software,
that may mean  that it is complicated to manipulate,  and  that  also
therefore means  that it is reserved for developers  and  experienced
professionals having in-depth computer knowledge. Users are therefore
encouraged to load and test the software's suitability as regards their
requirements in conditions enabling the security of their systems and/or
data to be ensured and,  more generally, to use and operate it in the
same conditions as regards security.

The fact that you are presently reading this means that you have had
knowledge of the CeCILL license and that you accept its terms.
*/


#ifndef GRAPHICSVIEW_HPP
#define GRAPHICSVIEW_HPP

#include <QGraphicsView>

/*!
 *  Inherits the Qt QGraphicsView class, permits to show a QGraphicsScene (a timebox in fullView).
 *  Manage the different types of mouse interactions and zooming changes (with + and - keys). @n
 *
 *  WIP : scaling automatically to fit a timebox in fullView.
 *
 *  @brief Widget displaying a Timebox
 *  @author Jaime Chao
 *  @date 2013/2014
 */
class GraphicsView : public QGraphicsView
{
  Q_OBJECT

public:
  explicit GraphicsView(QWidget *parent = 0);

signals:
  void mousePosition(QPointF); /// Used to emit mousePosition to the mainWindow's statusBar
  void mousePressAddItem(QPointF);

public slots:
  void mouseDragMode(QAction *); /// The DragMode property holds the behavior for dragging the mouse over the scene while the left mouse button is pressed.
  void graphicItemEnsureVisible(); /// Center the view on the graphicsitem's calling the slot

protected:
  // QWidget interface
  void mousePressEvent(QMouseEvent *); /// Send a signal to add a timebox in SmallView
  void mouseMoveEvent(QMouseEvent *); /// Send position of the mouse to mainwindow's statusBar
  void resizeEvent(QResizeEvent *);
  void keyPressEvent(QKeyEvent *event); /// Zooming with + and - keys

  // QGraphicsView interface
  void drawBackground(QPainter *painter, const QRectF &rect); /// Draw a filled brush pattern to show the space outside the scenario (temporary solution replacing fitFullView)

private:
  void fitFullView(); /// Arrange the FullView's container to fit inside the new space (called after a resize). @todo WIP
  void scaleView(qreal scaleFactor); /// Horizontal scaling with constraints checking
  void zoomIn();
  void zoomOut();
};

#endif // GRAPHICSVIEW_HPP
