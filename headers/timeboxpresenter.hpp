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

#ifndef TIMEBOXPRESENTER_HPP
#define TIMEBOXPRESENTER_HPP

class TimeboxModel;
class TimeboxSmallView;
class TimeboxFullView;
class TimeboxStorey;
class PluginView;
class GraphicsView;
class QGraphicsScene;
class QGraphicsRectItem;
class QFinalState;
class QStateMachine;
class QState;
class Timebox;
class StateDebug;

#include "timebox.hpp"
#include <QObject>
#include <unordered_map>
#include "utils.hpp"

/*!
 *  This class is the logic of a Timebox view, independently of the actual graphical representations of a Timebox ; which can be full, small or hide.
 *  This class drives the different states of a timebox and the permitted transitions beetween them.
 *
 *  @brief Timebox view logic
 *  @author Jaime Chao
 *  @date 2014
 */

class TimeboxPresenter : public QObject
{
  Q_OBJECT

private:
  ViewMode _mode;

  Timebox *_pTimebox;
  TimeboxModel *_pModel;
  TimeboxSmallView *_pSmallView = nullptr;
  TimeboxFullView *_pFullView = nullptr;
  GraphicsView *_pGraphicsView;

  std::unordered_map<TimeboxStorey*, PluginView*> _storeysSmallView;
  std::unordered_map<TimeboxStorey*, PluginView*> _storeysFullView;

  QStateMachine *_pStateMachine; /// Permits to maintaining state in complex applications
  QState *_pInitialState;
  QState *_pNormalState;
  StateDebug *_pSmallSizeState; /// When the graphical timeProcess is not occupying all size of the view
  StateDebug *_pFullSizeState; /// When the graphical timeProcess occupies all size of the view
  StateDebug *_pHideState; /// When the graphical timeProcess is not showed in the view
  QFinalState *_pFinalState;

public:
  TimeboxPresenter(TimeboxModel *pModel, TimeboxSmallView *pSmallView, GraphicsView *pView, Timebox *parent);
  TimeboxPresenter(TimeboxModel *pModel, TimeboxFullView *pFullView, GraphicsView *pView, Timebox *parent);
  ~TimeboxPresenter();

signals:
  void viewModeIsFull();
  void addBoxProxy(QGraphicsRectItem *rectItem); /// Proxy signal from Scenario plugin to Timebox
  void suppressTimeboxProxy();

public slots:
  void storeyBarButtonClicked(bool id);
  void goSmallView(); ///put public because of mainwindow header
  void goFullView();
  void goHide();

private slots:
  void init();
  void addStorey(int pluginType);
  PluginView* addPlugin(int pluginType, TimeboxStorey *pStorey);

public:
  TimeboxFullView* fullView() const {return _pFullView;}
  ViewMode mode() const {return _mode;}

private:
  void createFullView();
  void deleteStorey(TimeboxStorey* tbs);
  void createStates();
  void createTransitions();
  void createConnections();
  void createStateMachine();

};

#endif // TIMEBOXPRESENTER_HPP
