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

#ifndef PLUGINCURVEVIEW_HPP
#define PLUGINCURVEVIEW_HPP

#include <QPointF>
#include <QGraphicsRectItem>
#include <QCursor>

class QGraphicsItem;
class PluginCurvePresenter;
class PluginCurveGrid;
class PluginCurveMap;
class PluginCurveZoomer;

/*!
*  This class represent the grid.
*  Permits to paint the grid. @n
*
*  @brief Grid
*  @author Simon Touchard, Myriam Desainte-Catherine
*  @date 2014
*/

class PluginCurveView : public QGraphicsObject
{
        Q_OBJECT

// PluginCurveview = window, _pOrthoBasis = scene.

    private:
//  QGraphicsObject *_pParent; // Pointer to the parent deck
//  PluginCurvePresenter *_pPresenter; // Pointer to the presenter
        QGraphicsRectItem* _pSelectionRectangle; // Selection Rectangle
        PluginCurveZoomer* _pZoomer; // Contains all the others graphics element. Zoom effet will be applied on it.
    public:
        PluginCurveView(QGraphicsObject* parent);

        QGraphicsRectItem* selectionRectangle();
        PluginCurveZoomer* zoomer();
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget);
        QRectF boundingRect() const;

        void on_parentGeometryChange()
        {
            prepareGeometryChange();
        }



    signals:
        void doubleClicked(QGraphicsSceneMouseEvent*);
        void mousePressed(QGraphicsSceneMouseEvent*);
        void mouseMoved(QGraphicsSceneMouseEvent*);
        void mouseReleased(QGraphicsSceneMouseEvent*);
        void keyPressed(QKeyEvent*);
        void keyReleased(QKeyEvent*);
        void wheelTurned(QGraphicsSceneWheelEvent*);
        void viewSceneChanged(QGraphicsScene*);

    protected:
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event);
        void mousePressEvent(QGraphicsSceneMouseEvent* mouseEvent);
        void mouseMoveEvent(QGraphicsSceneMouseEvent* moveEvent);
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* releaseEvent);
        void keyPressEvent(QKeyEvent* keyEvent);
        void keyReleaseEvent(QKeyEvent* keyEvent);
        void wheelEvent(QGraphicsSceneWheelEvent* event);
        QVariant itemChange(GraphicsItemChange change, const QVariant& value);

    public slots:
        // Shows selection rectangle
        void startDrawSelectionRectangle(QPoint originSelectionRectangle);
        // Changes selection rectangle size
        void drawSelectionrectangle(QPoint originSelectionRectangle, QPoint destinationSelectionRectangle);
        // Hides selection rectangle
        void selectItems();
        // Changes cursor
        void changeCursor(QCursor cursor);

};

#endif // PLUGINCURVEVIEW_HPP
