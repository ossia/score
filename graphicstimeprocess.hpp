#ifndef GRAPHICSTIMEPROCESS_HPP
#define GRAPHICSTIMEPROCESS_HPP

/*! @file
 *  @brief Graphical representation of a TTTimeProcess class.
 *  @author Jaime Chao
 */

#include <QGraphicsObject>
#include <QStateMachine>
#include <QBrush>
#include "itemTypes.hpp"

class GraphicsTimeEvent;
class QGraphicsScene;
class QFinalState;

class GraphicsTimeProcess : public QGraphicsObject
{
  Q_OBJECT

  Q_PROPERTY(bool mainScenario READ mainScenario WRITE setmainScenario NOTIFY mainScenarioChanged)
  Q_PROPERTY(qreal width READ width WRITE setwidth NOTIFY widthChanged)
  Q_PROPERTY(qreal height READ height WRITE setheight NOTIFY heightChanged)
  Q_PROPERTY(bool running READ running WRITE setrunning NOTIFY runningChanged)
  Q_PROPERTY(bool paused READ paused WRITE setpaused NOTIFY pausedChanged)
  Q_PROPERTY(bool stopped READ stopped WRITE setstopped NOTIFY stoppedChanged)
  Q_PROPERTY(QBrush boxBrush READ boxBrush WRITE setboxBrush NOTIFY boxBrushChanged)
  //Q_PROPERTY(qint32 progression READ progression WRITE setprogression NOTIFY progressionChanged)

private:
  QGraphicsScene *_scene;
  /// @todo mettre les time event en Q_PROPERTY ?
  GraphicsTimeEvent *_startTimeEvent; /// The start timeEvent of the timeProcess
  GraphicsTimeEvent *_endTimeEvent; /// The end timeEvent of the timeProcess

  QStateMachine stateMachine; /// Permits to maintaining state in complex applications
  QState *initialState;
  QState *normalState;
  QState *editionState;
  QState *normalSizeState; /// When the graphical timeProcess is not occupying all size of the view
  QState *extendedSizeState; /// When the graphical timeProcess occupies all size of the view
  QState *layerSuppState; /// \todo gestion des étages et des layers
  QState *executionState;
  QState *runningState;
  QState *pausedState;
  QState *stoppedState;
  QFinalState *finalState;

  qreal m_width;
  qreal m_height;
  bool m_mainScenario;
  bool m_running;
  bool m_paused;
  bool m_stopped;

  QBrush m_boxEditingBrush;
  QBrush m_boxExecutionBrush;

  void createStates(const QPointF &position, QGraphicsItem *parent);

  QBrush m_boxBrush;

public:
  enum {Type = ProcessItemType};
  virtual int type() const {return Type;}

  explicit GraphicsTimeProcess(const QPointF &position, QGraphicsItem *parent, QGraphicsScene *scene);
  ~GraphicsTimeProcess();

  void createTransitions();

signals:
  void dirty();
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

protected:
  virtual QRectF boundingRect() const;
  virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  /// @todo keyPressEvent(QKeyEvent *event) voir p457 AQP

  //Property Getters
  bool mainScenario() const {return m_mainScenario;} /// @todo add "is" before method in case of boolean return
  qreal width() const {return m_width;}
  qreal height() const {return m_height;}
  bool running() const {return m_running;}
  bool paused() const {return m_paused;}
  bool stopped() const {return m_stopped;}
  QBrush boxBrush() const {return m_boxBrush;}

public slots:
  void setmainScenario(bool arg);
  void setwidth(qreal arg);
  void setheight(qreal arg);
  void setrunning(bool arg);
  void setpaused(bool arg);
  void setstopped(bool arg);
  void setboxBrush(QBrush arg);
};

#endif // GRAPHICSTIMEPROCESS_HPP
