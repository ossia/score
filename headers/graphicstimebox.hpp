/*
Copyright: LaBRI / SCRIME

Author: Jaime Chao (01/10/2013)

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

#ifndef GRAPHICSTIMEPROCESS_HPP
#define GRAPHICSTIMEPROCESS_HPP

///@todo Fusionner cette classe avec TimeBoxModel
/*! @file
 *  @brief Graphical representation of a TTTimeProcess class.
 *  @author Jaime Chao
 */

#include <QGraphicsObject>
#include <QBrush>
#include "itemTypes.hpp"

class TimeEvent;
class QGraphicsScene;
class QFinalState;
class QStateMachine;
class QState;

class GraphicsTimeBox : public QGraphicsObject
{
  Q_OBJECT

  Q_PROPERTY(bool mainScenario READ mainScenario WRITE setmainScenario NOTIFY mainScenarioChanged)
  Q_PROPERTY(qreal width READ width WRITE setwidth NOTIFY widthChanged)
  Q_PROPERTY(qreal height READ height WRITE setheight NOTIFY heightChanged)
  Q_PROPERTY(bool running READ running WRITE setrunning NOTIFY runningChanged) /// peut-être pas utile à maintenir
  Q_PROPERTY(bool paused READ paused WRITE setpaused NOTIFY pausedChanged) /// peut-être pas utile à maintenir
  Q_PROPERTY(bool stopped READ stopped WRITE setstopped NOTIFY stoppedChanged) /// peut-être pas utile à maintenir
  Q_PROPERTY(QBrush boxBrush READ boxBrush WRITE setboxBrush NOTIFY boxBrushChanged)
  Q_PROPERTY(QGraphicsScene* scene READ scene)

  //Q_PROPERTY(qint32 progression READ progression WRITE setprogression NOTIFY progressionChanged)

private:
  QGraphicsScene* _scene;
  /// @todo mettre les time event en Q_PROPERTY ?
  TimeEvent *_startTimeEvent; /// The start timeEvent of the timeProcess
  TimeEvent *_endTimeEvent; /// The end timeEvent of the timeProcess

  QStateMachine *_stateMachine; /// Permits to maintaining state in complex applications
  QState *_initialState;
  QState *_normalState;
  QState *_editionState;
  QState *_normalSizeState; /// When the graphical timeProcess is not occupying all size of the view
  QState *_extendedSizeState; /// When the graphical timeProcess occupies all size of the view
  QState *_layerSuppState; /// \todo gestion des étages et des layers
  QState *_executionState;
  QState *_runningState;
  QState *_pausedState;
  QState *_stoppedState;
  QFinalState *_finalState;

  qreal _width;
  qreal _height;
  bool _mainScenario;
  bool _running;
  bool _paused;
  bool _stopped;

  QBrush _boxEditingBrush;
  QBrush _boxExecutionBrush;
  QBrush _boxBrush;

public:
  GraphicsTimeBox(const QPointF &position = QPointF(0,0), const qreal width = 600, const qreal height = 400, QGraphicsItem *parent = 0);
  ~GraphicsTimeBox();

  enum {Type = BoxItemType}; //! Type value for custom item. Enable the use of qgraphicsitem_cast with this item
  virtual int type() const {return Type;}

signals:
  void dirty(); /// see AQP pagedesigner demo
  void mainScenarioChanged(bool arg);
  void widthChanged(qreal arg);
  void heightChanged(qreal arg);
  void runningChanged(bool arg);
  void pausedChanged(bool arg);
  void stoppedChanged(bool arg);
  void playOrPauseButtonClicked();
  void stopButtonClicked();
  void boxBrushChanged(QBrush arg);
  void suppress();
  void headerClicked();

public slots:
  void setmainScenario(bool arg);
  void setwidth(qreal arg);
  void setheight(qreal arg);
  void setrunning(bool arg);
  void setpaused(bool arg);
  void setstopped(bool arg);
  void setboxBrush(QBrush arg);
  void addPlugin(QGraphicsItem *item);

public:
  QGraphicsScene* scene() const {return _scene;}

protected:
  //Property Getters
  bool mainScenario() const {return _mainScenario;} /// @todo add "is" before method in case of boolean return
  qreal width() const {return _width;}
  qreal height() const {return _height;}
  bool running() const {return _running;}
  bool paused() const {return _paused;}
  bool stopped() const {return _stopped;}
  QBrush boxBrush() const {return _boxBrush;}

  //Graphic's Item interface
  void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event);
  virtual QRectF boundingRect() const;
  virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  /// @todo keyPressEvent(QKeyEvent *event) voir p457 AQP

private:
  void createScene();
  void createStates(const QPointF &position, QGraphicsItem *parent, qreal width, qreal height);
  void createTransitions();
};

#endif // GRAPHICSTIMEPROCESS_HPP
