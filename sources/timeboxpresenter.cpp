/*
Copyright: LaBRI / SCRIME

Authors : Jaime Chao, Clément Bossut (2013-2014)

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

#include "timeboxpresenter.hpp"
#include "automationview.hpp"

class TTTimeProcess;

#include "timeboxmodel.hpp"
#include "timeboxsmallview.hpp"
#include "timeboxfullview.hpp"
#include "timeboxstorey.hpp"
#include "pluginview.hpp"

#include <QGraphicsView>

#include <QDebug>

TimeboxPresenter::TimeboxPresenter(TimeboxModel *pModel, TimeboxSmallView *pSmallView)
  : _pModel(pModel), _pSmallView(pSmallView), _pFullView(NULL), _mode(SMALL)
{  
  connect(_pSmallView, SIGNAL(headerDoubleClicked()), this, SLOT(goFullView()));

  addStorey();
}

void TimeboxPresenter::storeyBarButtonClicked(bool id)
{
  Q_ASSERT(id == 0 || id == 1);
  TimeboxStorey* tbs = qobject_cast<TimeboxStorey*>(sender());
  if(id == 0) { /// We add a new storey
      addStorey();
      tbs->setButton(1);
    }
  else if(id == 1) {
      deleteStorey(tbs);
    }
}

void TimeboxPresenter::addStorey()
{
  TimeboxStorey *pStorey;

  switch (_mode) {
    case SMALL:
      pStorey = new TimeboxStorey(_pModel, _pModel->width(), 100, _pSmallView);
      _pSmallView->addStorey(pStorey);
      pStorey->setButton(0);
      _storeysSmallView.emplace(pStorey, new AutomationView(pStorey));
      _pModel->addPlugin();
      break;

    case FULL:
      pStorey = new TimeboxStorey(_pModel, _pView->width(), 200, _pFullView->pContainer());
      _pFullView->addStorey(pStorey);
      pStorey->setButton(0);
      _storeysFullView.emplace(pStorey, new AutomationView(pStorey));
      break;
    }

  connect(pStorey, SIGNAL(buttonClicked(bool)), this, SLOT(storeyBarButtonClicked(bool)));
}

void TimeboxPresenter::goSmallView()
{
  _mode = SMALL;
  _pView->setScene(_pParentScene);
}

void TimeboxPresenter::goFullView()
{
  _mode = FULL;
  if (_pFullView == NULL) {
      createFullView();
    }
  _pView->setScene(_pFullView);
}

void TimeboxPresenter::createFullView()
{
  _pFullView = new TimeboxFullView(_pModel, _pView);
  connect(_pFullView, SIGNAL(headerDoubleClicked()), this, SLOT(goSmallView()));

  std::list<TTTimeProcess*> lst = _pModel->pluginsFullView();

  /// @todo récupérer les plugins de smallsize
  //TimeboxStorey *pStorey;
  for (std::list<TTTimeProcess*>::iterator it = lst.begin() ; it != lst.end() ; ++it) { /// On crée un storey par plugin
     // pStorey = new TimeboxStorey(_pModel, _pModel->width());
     // _pFullView->addStorey(pStorey);
     // _storeysFullView.emplace(pStorey, new PluginView(pStorey));
      addStorey();
      /// @todo constructeur par copie des plugins pour aller dans full
    }
}

void TimeboxPresenter::deleteStorey(TimeboxStorey* tbs)
{
  std::unordered_map<TimeboxStorey*, PluginView*>::iterator it = _storeysSmallView.find(tbs);
  delete it->first;
  _storeysSmallView.erase(it);
  _pModel->removePlugin(); // TODO : Il faut permettre de supprimer le bon dans le modèle. et faire un delete pour fullView
}

