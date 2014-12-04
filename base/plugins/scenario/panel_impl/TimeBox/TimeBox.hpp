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

#ifndef TIMEBOX_HPP
#define TIMEBOX_HPP

class TimeboxModel;
class TimeboxPresenter;
class TimeboxFullView;
class TimeboxSmallView;
class GraphicsView;
class QGraphicsScene;
class TimeEvent;
class QString;

#include "Utils.hpp"
#include <QLineF>
#include <QRectF>
#include <QPointF>
#include <QObject>

/*!
 *  This class maintains together all the classes needed by a Timebox, offering a placeholder and makes interaction easier with Timebox object in i-score. @n
 *  This class drives the hierarchical changes and permits to maintain a consistent hierarchical relationship with other TimeBox.
 *
 *  @brief Timebox Interface
 *  @author Jaime Chao, Clément Bossut
 *  @date 2013/2014
*/
class Timebox : public QObject
{
		Q_OBJECT

	private:
		TimeboxSmallView* _pSmallView = nullptr;
		TimeboxPresenter* _pPresenter = nullptr;
		TimeboxModel* _pModel = nullptr;
		TimeboxFullView* _pFullView = nullptr;
		GraphicsView* _pGraphicsView; /// Pointer to the graphicsView's widget
		Timebox* _pParent = nullptr;  /// Pointer to the Timebox parent
//  TimeBarWidget *_timeBar = nullptr;
		static int staticId;          /// Give a unique number to each instance of Timebox

	public:
		explicit Timebox (Timebox* pParent, 
						  TimeEvent* pTimeEventStart, 
						  TimeEvent* pTimeEventEnd, 
						  GraphicsView* pView, 
						  QPointF pos, 
						  float width, 
						  float height, 
						  ViewMode mode, 
						  QString name = "");
		explicit Timebox (Timebox* pParent, 
						  GraphicsView* pView, 
						  QPointF pos, 
						  float width, 
						  float height, 
						  ViewMode mode, 
						  QString name = "");
		~Timebox();

	signals:
		void isFull();
		void isSmall();
		void isHide(); /// inform stateMachine's presenter that TimeBox is hidden

	private slots:
		void goFull(); /// bottom up (presenter -> mainwindow)
		void goHide();
		void createTimeEvent (QPointF pos);   /// Create a TimeEvent (signal emitted from ScenarioView)
		void removeTimeEvent (QPointF pos);   /// Removes a TimeEvent (signal emitted from ScenarioView)
		void createTimeboxAndTimeEvents (QRectF rect);   /// Create a Timebox and two surrounding TimeEvent (signal emitted from ScenarioView)
		void createTimeEventAndTimebox (QLineF line);   /// Click-drag in a already existing TimeEvent. Create another TimeEvent and a Timebox (signal emitted from TimeEventView)

	public slots:
		void goSmall(); /// Drive the hierarchical changes, instead of presenter.goSmallView() that check graphism. top-down (mainwindow -> presenter)

	public:
		// TODO NOT IN TIMEBOX.......
		void addChild (Timebox* other);       /// Add an already created Timebox to fullView
		void addChild (TimeEvent* TimeEvent); /// Add an already created TimeEvent to fullView
		TimeboxModel* model() const
		{
			return _pModel;   /// Used by GraphicsView's methods to retrieve width of the TimeBox
		}
		TimeboxFullView* fullView() const
		{
			return _pFullView;   /// Used by Mainwindow to retrieve the selected items
		}
		GraphicsView* getView() const
		{
			return _pGraphicsView;
		}

	private:
		// TODO no goddamn init method. Use constructor delegation instead
		void init (TimeEvent* pTimeEventStart, 
				   TimeEvent* pTimeEventEnd, 
				   const QPointF& pos, 
				   float height, 
				   float width, 
				   ViewMode mode, 
				   QString name);
};

#endif // TIMEBOX_HPP
