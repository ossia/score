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

#include "timebox.hpp"
#include "timeboxmodel.hpp"
#include "timeboxpresenter.hpp"
#include "timeboxsmallview.hpp"
#include "timeboxfullview.hpp"
#include "timeevent.hpp"
#include "timeeventmodel.hpp"
#include "timeeventview.hpp"
#include "mainwindow.hpp"
#include "graphicsview.hpp"

#include <QDebug>
#include <QApplication>

int Timebox::staticId = 1;

Timebox::Timebox(Timebox *pParent, TimeEvent *pTimeEventStart, TimeEvent *pTimeEventEnd, GraphicsView *pView, QPointF pos, float width, float height, ViewMode mode, QString name)
  : QObject(pParent), _pGraphicsView(pView), _pParent(pParent)
{
  init(pTimeEventStart, pTimeEventEnd, pos, height, width, mode, name);
}

Timebox::Timebox(Timebox *pParent, GraphicsView *pView, QPointF pos, float width, float height, ViewMode mode, QString name)
  : QObject(pParent), _pGraphicsView(pView), _pParent(pParent)
{
  TimeEvent *timeEventStart = new TimeEvent(pParent, pos);
  QPointF posEnd = pos + QPointF(width, 0);
  TimeEvent *timeEventEnd = new TimeEvent(pParent, posEnd);

  init(timeEventStart, timeEventEnd, pos, height, width, mode, name);
}

Timebox::~Timebox()
{
}

void Timebox::init(TimeEvent *pTimeEventStart, TimeEvent *pTimeEventEnd, const QPointF &pos, float height, float width, ViewMode mode, QString nameBox)
{
  ///@todo Vérifier si le nom existe déjà (par jC)
  if (nameBox.isEmpty()){
      /// If no name was given, we construct a name with a unique ID
      QString nameID = QString("Timebox%1").arg(staticId++);
      setObjectName(nameID);
    }

  _pModel = new TimeboxModel(pos.x(), pos.y(), width, height, objectName(), this, pTimeEventStart, pTimeEventEnd);

  if(mode == SMALL) {
      _pSmallView = new TimeboxSmallView(_pModel, this);
      _pPresenter = new TimeboxPresenter(_pModel, _pSmallView, _pGraphicsView, this);
    }
  else if(mode == FULL) {
      _pFullView = new TimeboxFullView(_pModel, this);
      _pPresenter = new TimeboxPresenter(_pModel, _pFullView, _pGraphicsView, this);
    }

  if(_pParent != nullptr) {
      if(mode == SMALL) {
          _pParent->addChild(this);
        }
    }

  connect(_pPresenter, SIGNAL(viewModeIsFull()), this, SLOT(goFull()));
  connect(_pPresenter, SIGNAL(createBoxProxy(QRectF)), this, SLOT(createTimeboxAndTimeEvents(QRectF)));
  connect(_pPresenter, SIGNAL(createTimeEventProxy(QPointF)), this, SLOT(createTimeEvent(QPointF)));
  connect(_pPresenter, SIGNAL(suppressTimeboxProxy()), this, SLOT(deleteLater()));

  MainWindow *window = qobject_cast<MainWindow*>(QApplication::activeWindow()); /// We retrieve a pointer to mainWindow
  if(window != NULL) {
      connect(this, SIGNAL(isFull()), window, SLOT(changeCurrentTimeboxScene())); /// Inform mainWindow (and particularly the headerWidget) that there is a new timeBox in fullView. Permits to jump to the parent later.
    }
  else {
      qWarning() << "Attention : Hierarchie vers la timebox parente est brisée !";
    }
}

void Timebox::goSmall()
{
  if (_pParent == nullptr) {
      /// Timebox with no parent is the mainTimebox.
      /// @todo Construct a parent Timebox and switch this one in smallView (and in sandbox mode) (by jC)
      return;
    }
  else {
      emit isSmall();
      /// @todo toutes mes timebox soeurs doivent devenir small (par jC)
      _pParent->goFull();
    }
}

void Timebox::goFull()
{
  if(_pParent != nullptr) {
      _pParent->goHide();
      _pParent->dumpObjectTree();
    }
  if(_pFullView == nullptr) { /// retrieve fullView the first time
      _pFullView = _pPresenter->fullView();
    }
  emit isFull();

  /// @todo toutes mes timebox soeurs doivent devenir hide
}

void Timebox::goHide()
{
  emit isHide();
}

void Timebox::createTimeEvent(QPointF pos)
{
  new TimeEvent(this, pos);
}

void Timebox::createTimeboxAndTimeEvents(QRectF rect)
{
  new Timebox(this, _pGraphicsView, rect.topLeft(), rect.width(), rect.height(), SMALL);
}

void Timebox::createTimeEventAndTimebox(QLineF line)
{
  TimeEvent *startTimeEvent, *endTimeEvent, *senderTimeEvent, *otherTimeEvent;
  QPointF posLeft;
  senderTimeEvent = qobject_cast<TimeEvent*>(QObject::sender());
  Q_ASSERT(senderTimeEvent != 0 );
  otherTimeEvent = new TimeEvent(qobject_cast<Timebox*>(senderTimeEvent->parent()), line.p2()); // They have the same timebox parent

  // check the direction of the click-drag
  if(line.dx() >= 0) {
      startTimeEvent = senderTimeEvent;
      endTimeEvent = otherTimeEvent;
      posLeft = line.p1();
    }
  else {
      startTimeEvent = otherTimeEvent;
      endTimeEvent = senderTimeEvent;
      posLeft = line.p2();
    }

  new Timebox(this, startTimeEvent, endTimeEvent, _pGraphicsView, posLeft, abs(line.dx()), 2*MIN_BOX_HEIGHT, SMALL);
}

void Timebox::addChild(Timebox *other)
{
  if(_pFullView == nullptr) {
      qWarning() << "Attention : Full View n'est pas crée !";
      return;
    }

  _pFullView->addItem(other->_pSmallView);
  _pFullView->clearSelection();
  other->_pSmallView->setSelected(true);
}

void Timebox::addChild(TimeEvent *timeEvent)
{
  if(_pFullView == nullptr) {
      qWarning() << "Attention : Full View n'est pas crée !";
      return;
    }

  _pFullView->addItem(timeEvent->view());

  /// @todo Problème avec la selection
  _pFullView->clearSelection();
  //timeEvent->setSelected(true);
}



