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

#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>

class QGraphicsScene;
class QActionGroup;
class QPointF;
class GraphicsTimeBox;
class QFinalState;
class QGraphicsView;
class QStateMachine;
class QState;
class TimeboxFullView;
class TimeboxPresenter;
class TimeboxSmallView;
class TimeboxModel;

namespace Ui {
  class MainWindow;
}

///@todo en faire une classe pour un constructeur et setteur propre et stable, les tests d'égalité et de parenté.
struct Timebox {
  TimeboxModel *pModel;
  TimeboxPresenter *pPresenter;
  TimeboxFullView *pFullView;
  TimeboxSmallView *pSmallView;
  QGraphicsScene *pScene; /// pointer to currentFullView->scene /// @todo J'ai peur qu'on rajoute trop d'attributs pour de simples raccourcis (à voir si on se servira souvent de _scene)
};

class MainWindow : public QMainWindow
{
  Q_OBJECT
  Q_PROPERTY(Timebox currentTimebox READ currentTimebox WRITE setcurrentTimebox NOTIFY currentTimeboxChanged) ///@todo est-ce utile de le garder en property ?

private:
  Timebox _mainTimebox;
  Timebox _currentTimebox;

  Ui::MainWindow *ui;
  QGraphicsView *_pView; /// pointer to ui->graphicsView
  QActionGroup *_pMouseActionGroup;

  ///@todo Ajouter un offset si on clique au meme endroit qu'un objet déjà ajouté
  /*
  qint16 _addOffset;
  QPoint _previousPoint;
  */

  QStateMachine *_stateMachine; /// Permits to maintaining state in complex applications
  QState *_initialState;
  QState *_normalState;
  QState *_editionState;
  QState *_executionState;
  QState *_runningState;
  QState *_pausedState;
  QState *_stoppedState;
  QFinalState *_finalState;

public:
  explicit MainWindow(QWidget *parent = 0);

public slots:
    void setDirty(bool on=true);
    void setMousePosition(QPointF point);

signals:
    void currentTimeboxChanged(Timebox arg);

private slots:
  void updateUi();
  void addItem(QPointF);

public:
  Timebox currentTimebox() const { return _currentTimebox; }
  void setcurrentTimebox(Timebox arg);

protected:
  // QWidget interface
  virtual void mousePressEvent(QMouseEvent *event);
  virtual void mouseMoveEvent(QMouseEvent *event);

private:
  void createGraphics();
  void createStates();
  void createTransitions();

  void connectItem(QObject *item);
  void createConnections();
  void createActionGroups();
};

#endif // MAINWINDOW_HPP
