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

#include "timeboxsmallview.hpp"
#include "timeboxfullview.hpp"
#include "timeboxstorey.hpp"
#include "pluginview.hpp"

TimeboxPresenter::TimeboxPresenter(TimeboxModel *pModel, TimeboxSmallView *pSmallView)
  : _pModel(pModel), _pSmallView(pSmallView), _pFullView(NULL), _mode(SMALL)
{  
  connect(_pSmallView, SIGNAL(headerDoubleClicked()), this, SLOT(goFullView()));

  _storeysSmallView.clear();
  addStorey();
}

void TimeboxPresenter::addStorey()
{
  TimeboxStorey *storey;

  switch (_mode) {
    case SMALL:
      storey = new TimeboxStorey(_pModel, _pSmallView);
      _pSmallView->addStorey(storey);
      _storeysSmallView.emplace(storey, new PluginView());
      break;

    case FULL:
      return; // TODO
    }

  connect(storey, SIGNAL(buttonAddClicked()), this, SLOT(addStorey()));
}

void TimeboxPresenter::goFullView()
{
  if (_pFullView == NULL) {
      createFullView();
    }
}

void TimeboxPresenter::createFullView()
{
  _pFullView = new TimeboxFullView(_pModel);

  _storeysFullView.clear();
}
