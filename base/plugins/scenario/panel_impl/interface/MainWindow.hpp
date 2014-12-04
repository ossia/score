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

#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QGraphicsScene>
#include "../TimeBox/TimeBox.hpp"
#include "TimeBarWidget.hpp"

class QGraphicsScene;
class QAction;
class QActionGroup;
class QPointF;
class GraphicsTimeBox;
class QFinalState;
class GraphicsView;
class QStateMachine;
class QState;
class TimeboxFullView;
class TimeboxPresenter;
class TimeboxSmallView;
class TimeboxModel;

namespace Ui
{
	class MainWindow;
}

/*!
 *  This class constructs the main interface of i-score, notably in relation with the designer's file "mainwindow.ui".
 *  A StateMachine permits to identify and drive the changes beetween the main states of i-score, like the user is in edition or execution mode.
 *  MainWindow is aware of the hierarchical changes of TimeBox, to maintain the _pCurrentTimebox (actual TimeBox in fullView), and permits to connect this TimeBox with the headerWiget.
 *
 *  @todo Creer une classe Editor pour regrouper GraphicsView, HeaderWidget, et la gestion des TimeBox. MainWindow ne devrait pas etre au courant et s'en occuper des TimeBox pour une meilleure séparation.
 *
 *  @brief i-score main class
 *  @author Jaime Chao
 *  @date 2013/2014
*/
class MainWindow : public QMainWindow
{
		Q_OBJECT

	private:
		Timebox* _pMainTimebox = nullptr;     /// Timebox parent
		Timebox* _pCurrentTimebox = nullptr;  /// Current Timebox in fullView (showed by GraphicsView)

		Ui::MainWindow* ui;   /// pointer to elements stored in mainwindow.ui
		GraphicsView* _pView; /// pointer to ui->graphicsView
		QAction* _pDeleteAction;
		QActionGroup* _pMouseActionGroup; /// actiongroup keeping all mouse relatives actions (mouse, scroll, select)

		TimeBarWidget* _timeBar = nullptr;

		QStateMachine* _stateMachine; /// Permits to maintaining state in complex applications. Especially for managing graphicals and interaction changes beetween execution and edition phases.
		QState* _initialState;
		QState* _normalState; /// parent state of edition and execution states
		QState* _editionState;
		QState* _executionState; /// parent state of running, paused and stopped state
		QState* _runningState;
		QState* _pausedState;
		QState* _stoppedState;
		QFinalState* _finalState;

	public:
		explicit MainWindow (QWidget* parent = 0);

	public slots:
		void setMousePosition (QPointF point);
		void changeCurrentTimeboxScene();

	private slots:
//  void addItem(QPointF); /// @todo Old way to add an item in the Timebox in fullView, now is directly in Scenario plugin. Could be deleted !
		void headerWidgetClicked(); /// Connect the headerWidget with the currentTimebox (in full view) and tell it to goSmall
		void deleteSelectedItems();

	public:
		Timebox* currentTimebox() const
		{
			return _pCurrentTimebox;
		}
		void setcurrentTimebox (Timebox* arg);

	private:
		void createGraphics(); /// Creation of the mainTimebox of the Scenario, the first one in fullView.
		void createActions(); /// The majority of actions are created in the Qt designer mainwindow.ui
		void createActionGroups();
		void createStates();
		void createTransitions();
		void createConnections();

};

#endif // MAINWINDOW_HPP
