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

#ifndef TIMEBOXMODEL_HPP
#define TIMEBOXMODEL_HPP

#include <List>
#include <QObject>
#include <QVector>

class TTTimeProcess;
class GraphicsTimeEvent;
class QGraphicsScene;
class QFinalState;
class QStateMachine;
class QState;
class QGraphicsItem;

/*!
 *  The model is linked with Score, and permits to maintain all elements used later by
 *  the other classes of Timebox API
 *
 *  @brief Maintain the model of a Timebox, no graphics here.
 *  \author Jaime Chao, Clément Bossut
 *  @date 2013/2014
*/
class TimeboxModel : public QObject
{
  Q_OBJECT

private:
  /// @todo creer les time event de début et de fin
  //GraphicsTimeEvent *_startTimeEvent; /// The start timeEvent of the timeProcess
  //GraphicsTimeEvent *_endTimeEvent; /// The end timeEvent of the timeProcess

  QStateMachine *_stateMachine; /// Permits to maintaining state in complex applications
  QState *_initialState;
  QState *_normalState;
  QState *_smallSizeState; /// When the graphical timeProcess is not occupying all size of the view
  QState *_fullSizeState; /// When the graphical timeProcess occupies all size of the view
  QFinalState *_finalState;

  int _time, _yPosition, _width, _height;
  std::list<TTTimeProcess*> _pluginsSmallView;
  std::list<TTTimeProcess*> _pluginsFullView;

public:
  TimeboxModel(int t = 0, int y = 0, int l = 100, int h = 100);
  ~TimeboxModel();

  int time() const {return _time;}
//  void setTime(int time);
  int yPosition() const {return _yPosition;}
//  void setYPosition(int yPosition);
  int width() const {return _width;}
//  void setLength(int length);
  int height() const {return _height;}
//  void setHeight(int height);

  const std::list<TTTimeProcess*>& pluginsSmallView() const {return _pluginsSmallView;}
  const std::list<TTTimeProcess*>& pluginsFullView() {
    if (_pluginsFullView.empty()) {
        _pluginsFullView.assign(_pluginsSmallView.begin(), _pluginsSmallView.end());
      }
    return _pluginsFullView;
  }

  void addPluginSmall();
  void removePluginSmall();
  void addPluginFull();
  void removePluginFull();

private:
  void createStates();
  void createTransitions();
  void createConnections();
  void createScene();

};

#endif // TIMEBOXMODEL_HPP
