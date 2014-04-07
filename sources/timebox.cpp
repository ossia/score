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
#include "mainwindow.hpp"
#include "graphicsview.hpp"
#include <QDebug>
#include <QGraphicsRectItem>
#include <QApplication>

Timebox::Timebox(Timebox *pParent)
  : QObject(pParent)
{
}

Timebox::Timebox(Timebox *pParent, GraphicsView *pGraphicsView, const QPointF &pos, float width, float height, ViewMode mode)
  : QObject(pParent), _pGraphicsView(pGraphicsView), _pParent(pParent)
{
  _pModel = new TimeboxModel(pos.x(), pos.y(), width, height); ///@todo faire le drag

  if(mode == SMALL) {
      _pSmallView = new TimeboxSmallView(_pModel);
      _pPresenter = new TimeboxPresenter(_pModel, _pSmallView, _pGraphicsView);
    }
  else if(mode == FULL) {
      _pFullView = new TimeboxFullView(_pModel);
      _pPresenter = new TimeboxPresenter(_pModel, _pFullView, _pGraphicsView);
      _pScene = _pFullView;
    }

  if(_pParent != NULL) {
      if(mode == SMALL) {
          _pParent->addChild(this);
        }
    }

  connect(_pPresenter, SIGNAL(viewModeIsFull()), this, SLOT(goFull()));
  connect(_pPresenter, SIGNAL(addBoxProxy(QGraphicsRectItem*)), this, SLOT(addChild(QGraphicsRectItem*)));
  MainWindow *window = qobject_cast<MainWindow*>(QApplication::activeWindow()); /// We retrieve a pointer to mainWindow
  if(window != NULL) {
      connect(this, SIGNAL(isFull()), window, SLOT(changeCurrentTimeboxScene())); /// Inform mainWindow (and particularly the headerWidget) that there is a new timeBox in fullView. Permits to jump to the parent later.
    }
  else {
      qWarning() << "Attention : Hierarchie vers la timebox parente est brisée !";
    }
}

Timebox::~Timebox()
{
  delete _pSmallView;
  delete _pFullView;
  delete _pModel;
  delete _pPresenter;
}

/// Drive the hierarchical changes, instead of presenter.goSmallView() that check graphism. top-down (mainwindow -> presenter)
void Timebox::goSmall()
{
  _pPresenter->goSmallView();
  _pParent->_pPresenter->goFullView();
}

/// bottom up (presenter -> mainwindow)
void Timebox::goFull()
{
  if(_pFullView == NULL) { /// retrieve fullView the first time
      _pFullView = _pPresenter->fullView();
    }
  emit isFull();
}

void Timebox::addChild (QGraphicsRectItem *rectItem)
{
  if(_pFullView == NULL) {
      qWarning() << "Attention : Full View n'est pas crée !";
      return;
    }
  new Timebox(this, _pGraphicsView, rectItem->pos(), rectItem->rect().width(), rectItem->rect().height(), SMALL);
}


void Timebox::addChild(Timebox *other)
{
  if(_pFullView == NULL) {
      qWarning() << "Attention : Full View n'est pas crée !";
      return;
    }
  _pFullView->addItem(other->_pSmallView);
  _pFullView->clearSelection();
  other->_pSmallView->setSelected(true);
}

void Timebox::addChild(TimeEvent *timeEvent)
{
  if(_pFullView == NULL) {
      qWarning() << "Attention : Full View n'est pas crée !";
      return;
    }
  _pFullView->addItem(timeEvent);
  _pFullView->clearSelection();
  timeEvent->setSelected(true);
}



