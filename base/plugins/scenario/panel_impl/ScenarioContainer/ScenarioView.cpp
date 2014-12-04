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

#include "ScenarioView.hpp"
#include "../TimeEvent/TimeEvent.hpp"
#include "MainWindow.hpp"
#include "../TimeBox/TimeBox.hpp"
#include "Utils.hpp"

#include <QGraphicsObject>
#include <QGraphicsSceneMouseEvent>
#include <QRectF>
#include <QPen>
#include <QBrush>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QDebug>

ScenarioView::ScenarioView (QGraphicsObject* parent)
	: PluginView (parent)
{
	setFlags (QGraphicsItem::ItemIsSelectable);
	setObjectName ("ScenarioView");
	setParent (parent);
}

void ScenarioView::mousePressEvent (QGraphicsSceneMouseEvent* mouseEvent)
{
	/// @todo Add test to check if no one is under the mousePosition. uncomment when done
	//scene()->views().first()->itemAt(mouseEvent->scenePos().toPoint()) == 0)

	// Testing that GraphicsView's DragMode property is NoDrag (ex: to avoid adding a Timebox in case of selection)
	if (scene()->views().first()->dragMode() == QGraphicsView::NoDrag)
	{
		if (mouseEvent->button() == Qt::LeftButton)
		{
			// Create an object in case of Command + LeftClick
			if (mouseEvent->modifiers() == Qt::CTRL)   // Qt::CTRL is equal to Command in Mac
			{

				if (_pTemporaryBox != nullptr)
				{
					delete _pTemporaryBox;
					_pTemporaryBox = nullptr;
				}

				// Store the first pressed point
				_pressPoint = mouseEvent->pos();

				// Add the temporary box to the scene
				_pTemporaryBox = new QGraphicsRectItem (QRectF (_pressPoint.x(), _pressPoint.y(), 0, 0), this);
				_pTemporaryBox->setPen (QPen (Qt::black) );
				_pTemporaryBox->setBrush (QBrush (Qt::NoBrush) );
			}
		}
	}

	QGraphicsObject::mousePressEvent (mouseEvent);
}

void ScenarioView::mouseMoveEvent (QGraphicsSceneMouseEvent* mouseEvent)
{
	if (_pTemporaryBox != nullptr)
	{
		int upLeftX, upLeftY, width, height;

		if (_pressPoint.x() < mouseEvent->pos().x() )
		{
			upLeftX = _pressPoint.x();
			width = mouseEvent->pos().x() - upLeftX;
		}
		else
		{
			upLeftX = mouseEvent->pos().x();
			width = _pressPoint.x() - upLeftX;
		}

		if (_pressPoint.y() < mouseEvent->pos().y() )
		{
			upLeftY = _pressPoint.y();
			height = mouseEvent->pos().y() - upLeftY;
		}
		else
		{
			upLeftY = mouseEvent->pos().y();
			height = _pressPoint.y() - upLeftY;
		}

		//If temporaryBox is inside the scenarioView
		if (boundingRect().contains (QRect (upLeftX, upLeftY, width, height) ) )
		{
			_pTemporaryBox->setRect (upLeftX, upLeftY, width, height);
		}
		else
		{
			delete _pTemporaryBox;
			_pTemporaryBox = nullptr;
		}
	}

	QGraphicsObject::mouseMoveEvent (mouseEvent);
}

void ScenarioView::mouseReleaseEvent (QGraphicsSceneMouseEvent* mouseEvent)
{

	if (_pTemporaryBox != nullptr)
	{
		//If temporaryBox is bigger enough we create a Timebox
		if (_pTemporaryBox->rect().width() > MIN_BOX_WIDTH && _pTemporaryBox->rect().height() > MIN_BOX_HEIGHT)
		{
			emit createTimebox (_pTemporaryBox->rect() );
		}
		else   // we create a TimeEvent
		{
			emit viewAskForTimeEvent (mouseEvent->pos() ); //
		}

		delete _pTemporaryBox;
		_pTemporaryBox = nullptr;
	}

	QGraphicsObject::mouseReleaseEvent (mouseEvent);
}

void ScenarioView::paint (QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
	//Re-implemented to not draw background like PluginView::paint()
	Q_UNUSED (option)
	Q_UNUSED (widget)

	painter->setPen (Qt::NoPen);
	painter->setBrush (Qt::NoBrush);
	painter->drawRect (boundingRect() );
}
