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

#include "PluginCurvePresenter.hpp"
#include "PluginCurveModel.hpp"
#include "PluginCurveView.hpp"

#include "Implementation/PluginCurveSection.hpp"
#include "Implementation/PluginCurveSectionLinear.hpp"
#include "Implementation/PluginCurvePoint.hpp"
#include "Implementation/PluginCurveMap.hpp"
#include "Implementation/PluginCurveGrid.hpp"
#include "Implementation/PluginCurveMenuPoint.hpp"
#include "Implementation/PluginCurveMenuSection.hpp"
#include "Implementation/PluginCurveZoomer.hpp"

#include <QGraphicsSceneEvent>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QCursor>
#include <QTransform>
#include <iostream>

PluginCurvePresenter::PluginCurvePresenter (PluginCurveModel* model,
											PluginCurveView* view,
											QObject* parent):
	QObject{parent},
	_pModel{model},
	_pView{view}
{
	// ** Initialisation **
	qreal minXValue = 0;
	qreal minYValue = -1;
	qreal maxXValue = 1;
	qreal maxYValue = 1;
	// Point's area
	/// @todo prendre l'echelle en paramètre du constructeur (?)
	// Point's area in scale coordinate
	_scale = QRectF (minXValue, minYValue, maxXValue - minXValue, maxYValue - minYValue);
	updateLimitRect();
	_pMap = new PluginCurveMap (_scale, _limitRect, this);
	_pGrid = new PluginCurveGrid (_pView->zoomer(), _pMap);
	// Points behavior
	//_pointCanCross = mainWindow()->pointCanCross();
	_pointCanCross = true;
	_magnetism = true;
	// ** Connections **
	// Presenter --> Model
	connect (this, SIGNAL (stateChanged (bool) ), _pModel, SLOT (setState (bool) ) );
	connect (this, SIGNAL (pointAdded (int, PluginCurvePoint*) ), _pModel, SLOT (pointInsert (int, PluginCurvePoint*) ) );
	connect (this, SIGNAL (pointRemoved (PluginCurvePoint*) ), _pModel, SLOT (pointRemoveOne (PluginCurvePoint*) ) );
	connect (this, SIGNAL (pointSwapped (int, int) ), _pModel, SLOT (pointSwap (int, int) ) );
	connect (this, SIGNAL (sectionAdded (PluginCurveSection*) ), _pModel, SLOT (sectionAppend (PluginCurveSection*) ) );
	connect (this, SIGNAL (sectionRemoved (PluginCurveSection*) ), _pModel, SLOT (sectionRemoveOne (PluginCurveSection*) ) );
	// Presenter --> view
	connect (this, SIGNAL (selectionStarted (QPoint) ), _pView, SLOT (startDrawSelectionRectangle (QPoint) ) );
	connect (this, SIGNAL (selectionMoved (QPoint, QPoint) ), _pView, SLOT (drawSelectionrectangle (QPoint, QPoint) ) );
	connect (this, SIGNAL (selectItems() ), _pView, SLOT (selectItems() ) );
	connect (this, SIGNAL (changeCursor (QCursor) ), _pView, SLOT (changeCursor (QCursor) ) );
	// View --> Presenter
	connect (_pView, SIGNAL (doubleClicked (QGraphicsSceneMouseEvent*) ), this, SLOT (doubleClick (QGraphicsSceneMouseEvent*) ) );
	connect (_pView, SIGNAL (mousePressed (QGraphicsSceneMouseEvent*) ), this, SLOT (mousePress (QGraphicsSceneMouseEvent*) ) );
	connect (_pView, SIGNAL (mouseMoved (QGraphicsSceneMouseEvent*) ), this, SLOT (mouseMove (QGraphicsSceneMouseEvent*) ) );
	connect (_pView, SIGNAL (mouseReleased (QGraphicsSceneMouseEvent*) ), this, SLOT (mouseRelease (QGraphicsSceneMouseEvent*) ) );
	connect (_pView, SIGNAL (keyPressed (QKeyEvent*) ), this, SLOT (keyPress (QKeyEvent*) ) );
	connect (_pView, SIGNAL (keyReleased (QKeyEvent*) ), this, SLOT (keyRelease (QKeyEvent*) ) );
	connect (_pView, SIGNAL (viewSceneChanged (QGraphicsScene*) ), this, SLOT (viewSceneChanged (QGraphicsScene*) ) );


	// NOTE : Uncomment for zooming on wheel
	// connect (_pView, SIGNAL (wheelTurned (QGraphicsSceneWheelEvent*) ), this, SLOT (wheelTurned (QGraphicsSceneWheelEvent*) ) );


	/*
	// Presenter --> PluginCurve
	connect (this, SIGNAL (notifyPointCreated (QPointF) ), parent, SIGNAL (notifyPointCreated (QPointF) ) );
	connect (this, SIGNAL (notifyPointDeleted (QPointF) ), parent, SIGNAL (notifyPointDeleted (QPointF) ) );
	connect (this, SIGNAL (notifyPointMoved (QPointF, QPointF) ), parent, SIGNAL (notifyPointMoved (QPointF, QPointF) ) );
	connect (this, SIGNAL (notifySectionCreated (QPointF, QPointF, qreal) ), parent, SIGNAL (notifySectionCreated (QPointF, QPointF, qreal) ) );
	connect (this, SIGNAL (notifySectionChanged (QPointF, QPointF, qreal) ), parent, SIGNAL (notifySectionChanged (QPointF, QPointF, qreal) ) );
	connect (this, SIGNAL (notifySectionDeleted (QPointF, QPointF) ), parent, SIGNAL (notifySectionDeleted (QPointF, QPointF) ) );
	connect (this, SIGNAL (notifySectionMoved (QPointF, QPointF, QPointF, QPointF) ), parent, SIGNAL (notifySectionMoved (QPointF, QPointF, QPointF, QPointF) ) );
	*/
}

///@todo customize cursors !!!
void PluginCurvePresenter::setEditionMode (EditionMode editionMode)
{
	QPoint origin;
	QPoint dest;

	switch (editionMode)
	{
		case CreationMode:
		case PenMode:
			emit (changeCursor (Qt::CrossCursor) );
			emit (setAllFlags (false) );
			break;

		case AreaSelectionMode:
			emit (changeCursor (Qt::ArrowCursor) );
			emit (setAllFlags (true) );
			break;

		case LinearSelectionMode:
			origin = QPoint (_originSelectionRectangle.x(), _pView->boundingRect().y() );

			//dest = QPoint(_pView->mapFromGlobal(QCursor::pos()).x(),_pView->boundingRect().y() +_pView->boundingRect().height());
			emit (changeCursor (Qt::IBeamCursor) );
			emit (selectionMoved (origin, dest) ); // If selection already start, change its size.
			emit (setAllFlags (true) );

		default:
			break;
	}

	_editionMode = editionMode;
}

void PluginCurvePresenter::setGridVisible (bool b)
{
	_pGrid->setVisible (b);
}

void PluginCurvePresenter::setMagnetism (bool b)
{
	_magnetism = b;
}

void PluginCurvePresenter::setPointCanCross (bool b)
{
	_pointCanCross = b;
}

void PluginCurvePresenter::adjustPoint (PluginCurvePoint* point, QPointF& newPos)
{
	Q_ASSERT (point != nullptr);
	PluginCurveSection* lSection = point->leftSection();
	PluginCurveSection* rSection = point->rightSection();
	PluginCurvePoint* previous = nullptr;
	PluginCurvePoint* next = nullptr;

	if (lSection != nullptr)
	{
		previous = lSection->sourcePoint();    // else nullptr
	}

	if (rSection != nullptr)
	{
		next = rSection->destPoint();    // else nullptr
	}

	// Schema :
	// (point)Previous -- (curve)lSection -- (point)This -- (curve)rSection -- (point)Next

	// Magnetism
	adjustPointMagnetism (newPos);
	// if the point is out the limits
	adjustPointLimit (newPos);
	//if the point can't move horizontally
	adjustPointMobility (point, newPos);
// ---->    point->setPos(newPos);

	// When the point reach another one
	// 3 curve must be modified.
	if (_pointCanCross)
	{
		if (next != nullptr && newPos.x() > next->x() )
		{
			crossByLeft (point, newPos);
		}

		if (previous != nullptr && newPos.x() < previous->x() )
		{
			crossByRight (point, newPos);
		}
	}

	//Respect the minimum distance between points
	adjustPointMinDist (point, newPos);
}

void PluginCurvePresenter::adjustPointMagnetism (QPointF& newPos)
{
	// NewPos : Zommer Coordinate
	if (_magnetism == true)
	{
		PluginCurveZoomer* zoomer = _pView->zoomer();
		QPointF magnetPoint = _pGrid->nearestMagnetPoint (newPos);
		QPointF magnetPointMap = _pView->mapFromItem (zoomer, magnetPoint);
		QPointF mapPos = _pView->mapFromItem (zoomer, newPos);

		if (qAbs (mapPos.x() - magnetPointMap.x() ) <= MAGNETDIST) // If the point is near th grid
		{
			newPos.setX (magnetPoint.x() );
		}

		if (qAbs (mapPos.y() - magnetPointMap.y() ) <= MAGNETDIST)
		{
			newPos.setY (magnetPoint.y() );
		}
	}
}

void PluginCurvePresenter::adjustPointMinDist (PluginCurvePoint* previousPoint, PluginCurvePoint* nextPoint, QPointF& newPos)
{
	QPointF nextMap (0, 0);
	QPointF previousMap (0, 0);
	PluginCurveZoomer* zoomer = _pView->zoomer();

	if (nextPoint != nullptr)
	{
		nextMap = zoomer->mapToItem (_pView, nextPoint->pos() );
	}

	if (previousPoint != nullptr)
	{
		previousMap = zoomer->mapToItem (_pView, previousPoint->pos() );
	}

	QPointF newPosMap = zoomer->mapToItem (_pView, newPos);

	if (previousPoint != nullptr && newPosMap.x() < previousMap.x() + POINTMINDIST)
	{
		previousMap.setX (previousMap.x() + POINTMINDIST);
		newPos.setX (zoomer->mapFromItem (_pView, previousMap).x() );
	}

	if (nextPoint != nullptr && newPosMap.x() > nextMap.x() - POINTMINDIST)
	{
		nextMap.setX (nextMap.x() - POINTMINDIST);
		newPos.setX (zoomer->mapFromItem (_pView, nextMap).x() );
	}

//    if (previousPoint != nullptr && newPos.x() < previousPoint->x() + POINTMINDIST)
//      newPos.setX(previousPoint->x() + POINTMINDIST);
//    if (nextPoint != nullptr && newPos.x() > nextPoint->x() - POINTMINDIST)
//      newPos.setX(nextPoint->x() - POINTMINDIST);
}


void PluginCurvePresenter::adjustPointMinDist (PluginCurvePoint* point, QPointF& newPos)
{
	PluginCurvePoint* previous = nullptr;
	PluginCurvePoint* next = nullptr;
	// point can't be nullptr.
	PluginCurveSection* lSection = point->leftSection();
	PluginCurveSection* rSection = point->rightSection();

	if (lSection != nullptr)
	{
		previous = lSection->sourcePoint();    // else nullptr
	}

	if (rSection != nullptr)
	{
		next = rSection->destPoint();    // else nullptr
	}

	adjustPointMinDist (previous, next, newPos);
}

void PluginCurvePresenter::adjustPointLimit (QPointF& newPos)
{
//    QRectF limitRect = _limitRect();
//    QRectF rect2 = _pView->mapRectFromItem(_limitRect,_limitRect); // define limit points positions
//    QRectF rect3 = _pView->transform().mapRect(_limitRect);
	QRectF rect = _pView->zoomer()->mapRectFromItem (_pView, _limitRect);

//    QRectF rectScene = _pView->mapRectFromScene(_limitRect->mapRectToScene(_limitRect));
	if (rect.x() > newPos.x() )
	{
		newPos.setX (rect.x() );
	}

	if (rect.y() > newPos.y() )
	{
		newPos.setY (rect.y() );
	}

	if (rect.x() + rect.width() < newPos.x() )
	{
		newPos.setX (rect.x() + rect.width() );
	}

	if (rect.y() + rect.height() < newPos.y() )
	{
		newPos.setY (rect.y() + rect.height() );
	}
}

void PluginCurvePresenter::adjustPointMobility (PluginCurvePoint* point, QPointF& newPos)
{
	if (point->mobility() == Vertical && point->fixedCoordinate() != newPos.x() )
	{
		//oldPos.setX(point->fixedCoordinate());
		newPos.setX (point->fixedCoordinate() );
	}
}

void PluginCurvePresenter::crossByLeft (PluginCurvePoint* point, QPointF& newPos)
{
	PluginCurveSection* lSection = point->leftSection();
	PluginCurveSection* rSection = point->rightSection();
	PluginCurveSection* tmpCurve = nullptr;  // The 3rd modified curve
	PluginCurvePoint* next = nullptr;
	int index = -1;

	// Schema :
	// (point)Previous -- (curve)lSection -- (point)This -- (curve)rSection -- (point)Next -- (curve)tmpCurve
	if (rSection != nullptr)
	{
		next = rSection->destPoint();    // else nullptr
	}

	if (next != nullptr)
	{
		tmpCurve = next->rightSection();    // else nullptr
	}

	// if no space
	if (tmpCurve == nullptr || rSection == nullptr || lSection == nullptr || !enoughSpaceAfter (next) )
	{
		newPos.setX (next->x() - POINTMINDIST);
	}
	else
	{
		//Modifie tmpCurve
		tmpCurve->setSourcePoint (point);
		point->setRightSection (tmpCurve);
		//Modifie lSection
		lSection->setDestPoint (next);
		next->setLeftSection (lSection);
		//Modifie rSection
		rSection->setSourcePoint (next);
		next->setRightSection (rSection);
		rSection->setDestPoint (point);
		point->setLeftSection (rSection);
		// Swap points in the list
		index = _pModel->pointIndexOf (point);
		emit (pointSwapped (index, index + 1) );
		// Set the point in the other side
		newPos.setX (next->x() + POINTMINDIST);
	}

	//point->adjust();
}

void PluginCurvePresenter::crossByRight (PluginCurvePoint* point, QPointF& newPos)
{
	PluginCurveSection* lSection = point->leftSection();
	PluginCurveSection* rSection = point->rightSection();
	PluginCurveSection* tmpCurve = nullptr;  // The 3rd modified curve
	PluginCurvePoint* previous = nullptr;
	int index = -1;

	// Schema :
	// (curve)tmpCurve -- (point)Previous -- (curve)lSection -- (point)This -- (curve)rSection -- (point)Next
	if (lSection != nullptr)
	{
		previous = lSection->sourcePoint();    // else nullptr
	}

	if (previous != nullptr)
	{
		tmpCurve = previous->leftSection();
	}

	if (tmpCurve == nullptr || rSection == nullptr || lSection == nullptr || !enoughSpaceBefore (previous) )
	{
		newPos.setX (previous->x() + POINTMINDIST);
	}
	else
	{
		//Modifie rSection
		rSection->setSourcePoint (previous);
		previous->setRightSection (rSection);
		//Modifie tmpCurve
		tmpCurve->setDestPoint (point);
		point->setLeftSection (tmpCurve);
		//Modifie lSection
		lSection->setSourcePoint (point);
		point->setRightSection (lSection);
		lSection->setDestPoint (previous);
		previous->setLeftSection (lSection);
		// Swap points in the list
		index = _pModel->pointIndexOf (point);
		emit (pointSwapped (index, index - 1) );
		// Set the point in the other side
		newPos.setX (previous->x() - POINTMINDIST);
	}

	//point->adjust();
}

bool PluginCurvePresenter::enoughSpaceBefore (PluginCurvePoint* point)
{
	PluginCurveZoomer* zoomer = _pView->zoomer();
	PluginCurvePoint* previous = _pModel->previousPoint (point);
	qreal pointX = _pView->mapFromItem (zoomer, point->pos() ).x();

	if (previous != NULL)
	{
		qreal previousX = _pView->mapFromItem (zoomer, previous->pos() ).x();
		return (pointX - previousX >= 2 * POINTMINDIST);
	}
	else
	{
		return (pointX - _limitRect.x() >= POINTMINDIST);
	}
}

bool PluginCurvePresenter::enoughSpaceAfter (PluginCurvePoint* point)
{
	PluginCurveZoomer* zoomer = _pView->zoomer();
	PluginCurvePoint* next = _pModel->nextPoint (point);
	qreal pointX = _pView->mapFromItem (zoomer, point->pos() ).x();

	if (next != NULL)
	{
		qreal nextX = _pView->mapFromItem (zoomer, next->pos() ).x();
		return (nextX - pointX >= 2 * POINTMINDIST);
	}
	else
	{
		return (_limitRect.x() + _limitRect.width() - pointX >= POINTMINDIST);
	}
}

void PluginCurvePresenter::updateLimitRect()
{
	_limitRect = QRectF (0 + PluginCurvePoint::SHAPERADIUS,
						 0 + PluginCurvePoint::SHAPERADIUS,
						 _pView->boundingRect().width() - 2 * PluginCurvePoint::SHAPERADIUS - 2,
						 _pView->boundingRect().height() - 2 * PluginCurvePoint::SHAPERADIUS);
}

PluginCurveSection* PluginCurvePresenter::addSection (PluginCurvePoint* source, PluginCurvePoint* dest)
{
	// Create the curve in the view
	PluginCurveSection* section = new PluginCurveSectionLinear (_pView->zoomer(), source, dest);
	source->setRightSection (section);
	dest->setLeftSection (section);
	// Emit signal for modifie model
	emit (sectionAdded (section) );
	emit (notifySectionCreated (section->sourcePoint()->getValue(), section->destPoint()->getValue(), section->bendingCoef() ) );
	connect (this, SIGNAL (setAllFlags (bool) ), section, SLOT (setAllFlags (bool) ) );
	connect (section, SIGNAL (doubleClicked (QGraphicsSceneMouseEvent*) ), this, SLOT (doubleClick (QGraphicsSceneMouseEvent*) ) );
	connect (section, SIGNAL (rightClicked (PluginCurveSection*, QPointF) ), this, SLOT (sectionRightClicked (PluginCurveSection*, QPointF) ) );
	return section;
}

#include <QDebug>
/// @todo Utiliser fonction déjà faites.
/// qpoint : view coordinates
PluginCurvePoint* PluginCurvePresenter::addPoint (QPointF qpoint, MobilityMode mobility, bool removable)
{
	QPointF newPos = qpoint;
	PluginCurvePoint* point = nullptr;
	PluginCurvePoint* previousPoint = nullptr;
	PluginCurvePoint* nextPoint = nullptr;
	PluginCurveSection* rightSection = nullptr;
	// The zoomer is the point's parent.
	PluginCurveZoomer* zoomer = _pView->zoomer();

	// No point added if out the aera
	/*
	if (!_limitRect.contains (qpoint) ) // qPoint view coordinate, _limitRect : view coordinate
	{
		return nullptr;
	}
	*/

	newPos = zoomer->mapFromItem (_pView, newPos); // Map the new position in zoomer's coordinates (paint coordinates).

	// Magnetism
	adjustPointMagnetism (newPos);
	// Where place the point in the list
	int index = _pModel->pointSearchIndex (newPos);

	// Determine the previous point
	if (index >= 0) // There is a previous point
	{
		previousPoint = _pModel->pointAt (index); // else nullptr
	}

	// Determine the next point
	if (index < _pModel->pointSize() - 1) // There is a next point
	{
		nextPoint = _pModel->pointAt (index + 1); // else nullptr
	}

	// No point added if not enough space
	if ( (previousPoint != nullptr && !enoughSpaceAfter (previousPoint) ) || // if there is a previous point and not enough space
			(nextPoint != nullptr && !enoughSpaceBefore (nextPoint) ) || //  or if there is a next point and not enough space
			(_limitRect.width() < 0) ) // or there is no points and no space at all
	{
		return nullptr;
	}

	// Correct the point position if too close of another one
	adjustPointMinDist (previousPoint, nextPoint, newPos);
	// Create the point
	point = new PluginCurvePoint (zoomer, this, newPos, _pMap->paintToScale (newPos), mobility, removable);
	emit (notifyPointCreated (_pMap->paintToScale (newPos) ) ); // Notify the user
//	emit (notifyPointCreated ( newPos ) ); // Notify the user

	//Create a new curve, update previousPoint and point.
	if (previousPoint != nullptr)
	{
		addSection (previousPoint, point);
	}

	/// @todo Creer une fonction setSourcePoint / setDestPoint (pour regrouper ces trois lignes) ???
	//Modifie the old one and updates the points.
	if (nextPoint != nullptr)
	{
		rightSection = nextPoint->leftSection();

		if (rightSection != nullptr)
		{
			QPointF oldSourcePoint =  previousPoint->getValue();
			QPointF oldDestPoint = nextPoint->getValue();
			rightSection->setSourcePoint (point);
			point->setRightSection (rightSection);
			emit (notifySectionMoved (oldSourcePoint, oldDestPoint, point->getValue(), oldDestPoint) );
		}
		else // No curve, a new one need to be created
		{
			rightSection = addSection (point, nextPoint);
		}
	}

	// Add points and curves in the view

	emit (pointAdded (index + 1, point) );
	connect (point, SIGNAL (rightClicked (PluginCurvePoint*) ), this, SLOT (pointRightClicked (PluginCurvePoint*) ) );
	connect (point, SIGNAL (pointPositionHasChanged() ), this, SLOT (pointPositionHasChanged() ) );
	connect (this, SIGNAL (setAllFlags (bool) ), point, SLOT (setAllFlags (bool) ) );
	return point;
}

void PluginCurvePresenter::removeSection (PluginCurveSection* section)
{
	if (section == nullptr)
	{
		return;
	}

	PluginCurvePoint* source = section->sourcePoint();
	PluginCurvePoint* dest = section->destPoint();

	if (source != nullptr)
	{
		source->setRightSection (nullptr);
	}

	if (source != nullptr)
	{
		dest->setLeftSection (nullptr);
	}

	//section->scene()->removeItem(section);
	emit (sectionRemoved (section) );
	emit (notifySectionDeleted (section->sourcePoint()->getValue(), section->destPoint()->getValue() ) );
	//section->hide();
	section->deleteLater();
}

void PluginCurvePresenter::removePoint (PluginCurvePoint* point)
{
	if (point == nullptr)
	{
		return;
	}

	point->setSelected (false);
	PluginCurvePoint* previous = nullptr;
	PluginCurveSection* leftSection = point->leftSection();
	PluginCurveSection* rightSection = point->rightSection();

//  point->setRightSection(nullptr);
//  point->setLeftSection(nullptr);
	if (leftSection != nullptr) // There is a previous point
	{
		previous = leftSection->sourcePoint(); // else NULL
		removeSection (leftSection); // Delete the left curve
		leftSection = nullptr; // Pointer useless now
	}

	if (rightSection != nullptr)
	{
		//next = rightSection->destPoint(); // else Null
		if (previous != nullptr) // There is a previous point
		{
			rightSection->setSourcePoint (previous); // Keep the right curve, change its source point
			rightSection->adjust();
		}
		else // No previous point
		{
			removeSection (rightSection);
			rightSection = nullptr; // Pointer useless now
		}
	}

	if (previous != nullptr)
	{
		previous->setRightSection (rightSection);
	}

//  if (next != nullptr)
//    {
//      next->setLeftSection(rightSection);
//    }
	emit (pointRemoved (point) );
	emit (notifyPointDeleted (point->getValue() ) );
	//point->scene()->removeItem(point);
	//point->hide();
	point->deleteLater();
}

void PluginCurvePresenter::doubleClick (QGraphicsSceneMouseEvent* mouseEvent)
{
	if (mouseEvent->button() == Qt::LeftButton)
	{
		_pView->scene()->clearSelection();
//      addPoint(_pView->mapFromScene(QPointF(10,10)));
		_lastCreatedPoint = addPoint (_pView->mapFromScene (mouseEvent->scenePos() ) ); // ->pos() isn't correct here
		emit (setAllFlags (true) );
	}
}

void PluginCurvePresenter::mousePress (QGraphicsSceneMouseEvent* mouseEvent)
{
	switch (_editionMode)
	{
		case CreationMode :
		case PenMode :
			if (mouseEvent->button() == Qt::LeftButton)
			{
				_pView->scene()->clearSelection();
				_lastCreatedPoint = addPoint (mouseEvent->pos() );
				emit (setAllFlags (true) );
			}

			break;

		case AreaSelectionMode :
			mouseEvent->accept();

			if (mouseEvent->button() == Qt::LeftButton)
			{
				_originSelectionRectangle = mouseEvent->pos().toPoint();
				_pView->scene()->clearSelection();
				emit (selectionStarted (_originSelectionRectangle) );
			}

			break;

		case LinearSelectionMode :
			mouseEvent->accept();

			if (mouseEvent->button() == Qt::LeftButton)
			{
				_originSelectionRectangle = mouseEvent->pos().toPoint();
				_pView->scene()->clearSelection();
				emit (selectionStarted (QPoint (_originSelectionRectangle.x(), _pView->boundingRect().y() ) ) );
			}

			break;

		default :
			break;
	}
}

void PluginCurvePresenter::mouseMove (QGraphicsSceneMouseEvent* mouseEvent)
{
	QPoint destinationSelectionRectangle;
	QRect boundingRect = _pView->boundingRect().toRect();

	switch (_editionMode)
	{
		case CreationMode :
			if (_lastCreatedPoint != nullptr)
			{
				_lastCreatedPoint->setPos (mouseEvent->pos() );
			}

			break;

		case PenMode :
			addPoint (mouseEvent->pos() );
			break;

		case AreaSelectionMode :
			destinationSelectionRectangle = mouseEvent->pos().toPoint();
			emit (selectionMoved (_originSelectionRectangle, destinationSelectionRectangle) );
			break;

		case LinearSelectionMode :
			destinationSelectionRectangle = QPoint (mouseEvent->pos().x(), boundingRect.y() + boundingRect.height() );
			emit (selectionMoved (QPoint (_originSelectionRectangle.x(), boundingRect.y() ), destinationSelectionRectangle) );
			break;

		default :
			break;
	}
}

void PluginCurvePresenter::mouseRelease (QGraphicsSceneMouseEvent* mouseEvent)
{
	switch (_editionMode)
	{
		case CreationMode :
			if (mouseEvent->button() == Qt::LeftButton)
			{
				_lastCreatedPoint = nullptr;
			}

			break;

		case AreaSelectionMode :
		case LinearSelectionMode :
			if (mouseEvent->button() == Qt::LeftButton)
			{
				emit (selectItems() );
			}

			break;

		default :
			break;
	}
}

void PluginCurvePresenter::keyPress (QKeyEvent* keyEvent)
{
	switch (keyEvent->key() )
	{
		case Qt::Key_Shift:
		{
			setEditionMode (LinearSelectionMode);
			break;
		}

		case Qt::Key_Alt :
		{
			setEditionMode (PenMode);
			break;
		}

		case Qt::Key_Backspace :
		{
			QListIterator<PluginCurvePoint*> iterator (_pModel->points() );
			PluginCurvePoint* point;

			//QGraphicsItem *item;
			while (iterator.hasNext() )
			{
				point = iterator.next();

				if (point != nullptr && point->removable() && point->isSelected() )
				{
					removePoint (point);
				}
			}

			break;
		}

		case Qt::Key_Right:
		{
			_pView->zoomer()->translateX (-5);
			_pMap->setPaintRect (_pView->zoomer()->mapRectFromItem (_pView, _limitRect) );
			break;
		}

		case Qt::Key_Left:
		{
			_pView->zoomer()->translateX (+5);
			_pMap->setPaintRect (_pView->zoomer()->mapRectFromItem (_pView, _limitRect) );
			break;
		}

		case Qt::Key_Up:
		{
			_pView->zoomer()->translateY (+5);
			_pMap->setPaintRect (_pView->zoomer()->mapRectFromItem (_pView, _limitRect) );
			break;
		}

		case Qt::Key_Down:
		{
			_pView->zoomer()->translateY (-5);
			_pMap->setPaintRect (_pView->zoomer()->mapRectFromItem (_pView, _limitRect) );
			break;
		}

		default:
			break;
	}

	//QGraphicsObject::keyPressEvent(keyEvent);
}

void PluginCurvePresenter::keyRelease (QKeyEvent* keyEvent)
{
	switch (keyEvent->key() )
	{
		case Qt::Key_Alt:
		case Qt::Key_Shift:
			//setEditionMode(mainwindow()->editionMode());
			setEditionMode (AreaSelectionMode);
			break;

		default :
			break;
	}

	// QGraphicsObject::keyReleaseEvent(keyEvent);
}

void PluginCurvePresenter::wheelTurned (QGraphicsSceneWheelEvent* event)
{
	PluginCurveZoomer* zoomer = _pView->zoomer();
	QPointF origin = _pView->zoomer()->mapFromItem (_pView, event->pos() );
	_pView->zoomer()->zoom (origin, event->delta() );
	// Limit rect coordinate have changed in zoomer's coordinate system
	_pMap->setPaintRect (zoomer->mapRectFromItem (_pView, _limitRect) ); // Update map and grid
}

void PluginCurvePresenter::viewSceneChanged (QGraphicsScene* scene)
{
	Q_UNUSED (scene);
	_limitRect = QRectF (0 + PluginCurvePoint::SHAPERADIUS,
						 0 + PluginCurvePoint::SHAPERADIUS,
						 _pView->boundingRect().width() - 2 * PluginCurvePoint::SHAPERADIUS - 2,
						 _pView->boundingRect().height() - 2 * PluginCurvePoint::SHAPERADIUS);
	_pMap->setPaintRect (_pView->zoomer()->mapRectFromItem (_pView, _limitRect), false); // No scale rect change
}

void PluginCurvePresenter::pointPositionHasChanged()
{
	QListIterator<PluginCurvePoint*> iterator (_pModel->points() );
	PluginCurvePoint* point;

	//QGraphicsItem *item;
	while (iterator.hasNext() )
	{
		point = iterator.next();

		if (point != nullptr && point->isSelected() )
		{
			// Update the point value
			QPointF oldvalue = point->getValue();
			point->setValue (_pMap->paintToScale (point->pos() ) );
			// Notify the plugin users
			emit (notifyPointMoved (oldvalue, point->getValue() ) );
			//emit(notifySectionMoved());
		}
	}

}

void PluginCurvePresenter::pointRightClicked (PluginCurvePoint* point)
{
	PluginCurveMenuPoint menu (point);
	QAction* selectedItem = menu.exec (point->globalPos().toPoint() );

	if (selectedItem)
	{
		if (selectedItem->text() == MENUPOINT_DELETE_TEXT)
		{
			removePoint (point);
		}

		if (selectedItem->text() == MENUPOINT_FIX_HORIZONTAL_TEXT)
		{
			if (!selectedItem->isChecked() )
			{
				point->setMobility (Normal);
			}
			else
			{
				point->setMobility (Vertical);
			}

			selectedItem->setChecked (!selectedItem->isChecked() );
		}
	}
	else
	{
		//Do nothing
	}
}

void PluginCurvePresenter::sectionRightClicked (PluginCurveSection* section, QPointF scenePos)
{
	// Get the global pos of the Cursor
	Q_ASSERT (section->scene() != NULL); // the focus item belongs to a scene
	Q_ASSERT (!section->scene()->views().isEmpty() ); // that scene is displayed in a view...
	Q_ASSERT (section->scene()->views().first() != NULL); // ... which is not null...
	Q_ASSERT (section->scene()->views().first()->viewport() != NULL); // ... and has a viewport
	QGraphicsView* v = section->scene()->views().first();
	QPointF globalPos = v->viewport()->mapToGlobal (v->mapFromScene (scenePos) );
	PluginCurveMenuSection menu (section);
	QAction* action = menu.exec (globalPos.toPoint() );

	if (action->text() == PluginCurveMenuSection::DELETE)
	{
		removePoint (section->sourcePoint() );
		removePoint (section->destPoint() );
	}
}
