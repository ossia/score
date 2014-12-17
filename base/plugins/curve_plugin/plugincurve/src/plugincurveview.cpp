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

#include "../include/plugincurveview.hpp"
#include "../include/plugincurvegrid.hpp"
#include "../include/plugincurvepresenter.hpp"
#include "../include/plugincurvemap.hpp"
#include "../include/plugincurvezoomer.hpp"
#include <QPainter>
#include <QRectF>
#include <QGraphicsScene>
#include <QCursor>
#include <iostream>


PluginCurveView::PluginCurveView (QGraphicsObject* parent)
	: ProcessViewInterface ()
{
	_pZoomer = new PluginCurveZoomer (this);
	_pSelectionRectangle = new QGraphicsRectItem (QRect (QPoint(), QSize() ), this);
	_pSelectionRectangle->setFlag (ItemIgnoresTransformations);
	_pSelectionRectangle->hide();
	setFlag (ItemIsFocusable); // For board entries
	setFlag (ItemClipsChildrenToShape); // Children can't be drawn outside this item's shape
	setFocus(); /// @todo get focus ? Good idea ? Create fonctions for get / release focus ?
}

PluginCurveView::~PluginCurveView()
{
	delete _pSelectionRectangle;
	delete _pZoomer;
}

PluginCurveZoomer* PluginCurveView::zoomer()
{
	return _pZoomer;
}

QGraphicsRectItem* PluginCurveView::selectionRectangle()
{
	return _pSelectionRectangle;
}

void PluginCurveView::paint (QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Q_UNUSED (option)
	Q_UNUSED (widget)
	Q_UNUSED (painter)

}

/// @todo corriger si scene rect non défini !
QRectF PluginCurveView::boundingRect() const
{
	if (parentItem() == nullptr)
	{
		if (scene() == nullptr)
		{
			return QRectF (0, 0, 0, 0);
		}
		else
		{
			return mapFromScene (scene()->sceneRect() ).boundingRect();
		}
	}
	else
	{
		return mapFromParent (parentItem()->boundingRect() ).boundingRect();
	}
}

void PluginCurveView::mouseDoubleClickEvent (QGraphicsSceneMouseEvent* event)
{
	emit (doubleClicked (event) );
}

void PluginCurveView::mousePressEvent (QGraphicsSceneMouseEvent* mouseEvent)
{
	emit (mousePressed (mouseEvent) );
}

void PluginCurveView::mouseMoveEvent (QGraphicsSceneMouseEvent* moveEvent)
{
	emit (mouseMoved (moveEvent) );
}

void PluginCurveView::mouseReleaseEvent (QGraphicsSceneMouseEvent* releaseEvent)
{
	emit (mouseReleased (releaseEvent) );
}

void PluginCurveView::keyPressEvent (QKeyEvent* keyEvent)
{
	emit (keyPressed (keyEvent) );
}

void PluginCurveView::keyReleaseEvent (QKeyEvent* keyEvent)
{
	emit (keyReleased (keyEvent) );
}

void PluginCurveView::wheelEvent (QGraphicsSceneWheelEvent* event)
{
	emit (wheelTurned (event) );
}

QVariant PluginCurveView::itemChange (GraphicsItemChange change, const QVariant& value)
{
	switch (change)
	{
		case ItemSceneHasChanged:
			//_pZoomer->prepareGeometryChange();
			emit (viewSceneChanged (scene() ) ); // The new scene is emitted
			break;

		default:
			return QGraphicsItem::itemChange (change, value);
	}

	return QGraphicsItem::itemChange (change, value);
}

void PluginCurveView::startDrawSelectionRectangle (QPoint originSelectionRectangle)
{
	QRect rect;
	rect = QRect (originSelectionRectangle, originSelectionRectangle).normalized().intersected (boundingRect().toRect() );
	//Q_ASSERT(transform().isInvertible());
	rect = transform().mapRect (rect);
	_pSelectionRectangle->setRect (rect);
	_pSelectionRectangle->show();
}

void PluginCurveView::drawSelectionrectangle (QPoint originSelectionRectangle, QPoint destinationSelectionRectangle)
{
	QRect rect;
	QPainterPath painterPath;
	rect = QRect (originSelectionRectangle, destinationSelectionRectangle).normalized().intersected (boundingRect().toRect() );
	rect = transform().mapRect (rect); // Map the rectangle in the correct scale
	_pSelectionRectangle->setRect (rect);
	painterPath.addPolygon (mapToScene (transform().mapRect (rect) ).toPolygon() );
	//Select the items
	scene()->setSelectionArea (painterPath, Qt::IntersectsItemShape);
}

///@todo Changer Nom methode
void PluginCurveView::selectItems()
{
	_pSelectionRectangle->hide();
}

void PluginCurveView::changeCursor (QCursor cursor)
{
	setCursor (cursor);
}
