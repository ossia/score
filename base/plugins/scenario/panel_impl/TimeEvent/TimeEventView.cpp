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

#include "TimeEventView.hpp"
#include "TimeEventPresenter.hpp"
#include "TimeEvent.hpp"
#include "MainWindow.hpp"
#include "Utils.hpp"

#include <QPen>
#include <QPainter>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsSceneMouseEvent>
#include <QDebug>


TimeEventPresenter* TimeEventView::presenter() const
{
	return m_presenter;
}

void TimeEventView::setPresenter(TimeEventPresenter* presenter)
{
	m_presenter = presenter;
}
TimeEventView::TimeEventView (int index, 
							  QObject* parentObject, 
							  QGraphicsItem* parentGraphics) :
	QGraphicsObject (parentGraphics), 
	m_index{index},
	_penWidth (1), 
	_circleRadii (10), 
	_height (0)
{
	setFlags (QGraphicsItem::ItemIsSelectable |
	          QGraphicsItem::ItemIsMovable |
	          QGraphicsItem::ItemSendsScenePositionChanges |
	          QGraphicsItem::ItemSendsGeometryChanges);

	setParent (parentObject); ///@todo vérifier si ça ne pose pas problème d'avoir un parent graphique et object différents ?
	setZValue (1); // Draw on top of Timebox
	setSelected (true);

}

TimeEventView::~TimeEventView()
{
}

QVariant TimeEventView::itemChange (GraphicsItemChange change, const QVariant& value)
{
	if (change == ItemPositionChange && scene() )
	{
		QPointF newPos = value.toPointF();  // value is the new position

		QRectF rect = scene()->sceneRect();
		QRectF bRectMoved = boundingRect();
		bRectMoved.moveTo (newPos);

		if (!rect.contains (bRectMoved) ) // if item exceed plugin scenario we keep the item inside the scene rect
		{
			newPos.setX (qMin (rect.right(), qMax (newPos.x(), rect.left() ) ) );
			newPos.setY (qMin (rect.bottom(), qMax (newPos.y(), rect.top() ) ) );
		}

		emit xChanged (newPos.x() ); // Inform the model
		emit yChanged (newPos.y() );

		return newPos;
	}

	return QGraphicsObject::itemChange (change, value);
}

void TimeEventView::mousePressEvent (QGraphicsSceneMouseEvent* mouseEvent)
{
	// Testing that GraphicsView's DragMode property is NoDrag
	if (scene()->views().first()->dragMode() == QGraphicsView::NoDrag)
	{
		if (mouseEvent->button() == Qt::LeftButton)
		{
			if (mouseEvent->modifiers() == Qt::SHIFT)
			{

				if (_pTemporaryRelation != nullptr)
				{
					delete _pTemporaryRelation;
					_pTemporaryRelation = nullptr;
				}

				// Add the temporary relation to the scene
				_pTemporaryRelation = new QGraphicsLineItem (QLineF (0, 0, 0, 0), this);
				_pTemporaryRelation->setPen (QPen (Qt::black) );
			}

		}
	}

	QGraphicsObject::mousePressEvent (mouseEvent);
}

void TimeEventView::mouseMoveEvent (QGraphicsSceneMouseEvent* mouseEvent)
{
	if (_pTemporaryRelation != nullptr)
	{
		int LeftX, width;

		if (mouseEvent->pos().x() > 0)
		{
			LeftX = 0;
			width = mouseEvent->pos().x();
		}
		else
		{
			LeftX = mouseEvent->pos().x();
			width = - LeftX;
		}

		// If temporaryBox is inside the scenarioView
		if (scene()->sceneRect().contains (mapToScene (mouseEvent->pos() ) ) )
		{
			_pTemporaryRelation->setLine (0, 0, mouseEvent->pos().x(), 0);
		}
		else
		{
			delete _pTemporaryRelation;
			_pTemporaryRelation = nullptr;
		}
	}
	else   // else we move the TimeEvent
	{
		QGraphicsObject::mouseMoveEvent (mouseEvent);
	}
}

void TimeEventView::mouseReleaseEvent (QGraphicsSceneMouseEvent* mouseEvent)
{
	if (_pTemporaryRelation != nullptr)
	{
		//If temporaryRelation is bigger enough
		if (abs (_pTemporaryRelation->line().dx() ) > MIN_BOX_WIDTH)
		{
			QLineF line = _pTemporaryRelation->line();
			line.translate (mapToScene (line.p1() ) ); /// We need to map the position to the coordinate system of the parent scene (Timebox in fullView)
			emit createTimeEventAndTimebox (line);
		}

		delete _pTemporaryRelation;
		_pTemporaryRelation = nullptr;
	}

	QGraphicsObject::mouseReleaseEvent (mouseEvent);
}

QRectF TimeEventView::boundingRect() const
{
	return QRectF (-_circleRadii - _penWidth / 2, -_circleRadii - _penWidth / 2,
	               2 * _circleRadii + _penWidth, 2 * _circleRadii + _height + _penWidth);
}

void TimeEventView::paint (QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	Q_UNUSED (option)
	Q_UNUSED (widget)

	QPen pen (Qt::SolidPattern, _penWidth);
	pen.setCosmetic (true);
	painter->setPen (pen);
	painter->drawLine (0, _circleRadii, 0, _circleRadii + _height);
	painter->drawEllipse (QPointF (0, 0), _circleRadii, _circleRadii);
}

QPainterPath TimeEventView::shape() const
{
	QPainterPath path;
	path.addEllipse (QPointF (0, 0), _circleRadii, _circleRadii);
	path.addRect (0, _circleRadii, _penWidth, _height); /// We can select the object 1 pixel surrounding the line
	return path;
}

void TimeEventView::setY (qreal arg)
{
	if (pos().y() != arg)
	{
		setPos (x(), arg);
	}
}

void TimeEventView::setX (qreal arg)
{
	if (pos().x() != arg)
	{
		setPos (arg, y() );
	}
}
