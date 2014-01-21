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
#include <QGraphicsView>
#include <QDebug>

Timebox::Timebox(Timebox *pParent)
  : QObject(pParent)
{
}

Timebox::Timebox(Timebox *pParent, QGraphicsView *pView, const QPointF &pos, float width, float height, ViewMode mode)
  : QObject(pParent), _pView(pView)
{
  _pModel = new TimeboxModel(pos.x(), pos.y(), width, height); ///@todo faire le drag

  if(mode == SMALL) {
      _pSmallView = new TimeboxSmallView(_pModel);
      _pPresenter = new TimeboxPresenter(_pModel, _pSmallView, pView);
    }
  else if(mode == FULL) {
      _pFullView = new TimeboxFullView(_pModel);
      _pPresenter = new TimeboxPresenter(_pModel, _pFullView, pView);
      _pScene = _pFullView;
    }

  if(pParent != NULL) {
      _pPresenter->setParentScene(pParent->_pFullView);
      if(mode == SMALL) {
          pParent->addChild(this);
        }
    }

  connect(_pPresenter, SIGNAL(timeboxFullChanged()), this, SIGNAL(timeboxBecameFull()));
}

Timebox::~Timebox()
{
  delete _pSmallView;
  delete _pFullView;
  delete _pModel;
  delete _pPresenter;
}

bool Timebox::isEqual(const Timebox &other) ///@bug ne fonctionne pas ?!
{
  return _pModel==other._pModel && _pPresenter==other._pPresenter;
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



