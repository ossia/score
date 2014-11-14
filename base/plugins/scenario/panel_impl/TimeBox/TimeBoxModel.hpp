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

#include <list>
#include <QObject>
#include <QVector>
#include <QString>

class TTTimeProcess;
class Timebox;
class TimeEventModel;

/*!
 *  The model is linked with OSSIA API, and permits to maintain all elements used later by
 *  the other classes of Timebox.
 *
 *  @brief Maintain the model of a Timebox, no graphics here.
 *  @author Jaime Chao, Clément Bossut
 *  @date 2013/2014
*/

class TimeboxModel : public QObject
{
		Q_OBJECT

		Q_PROPERTY (qreal _time READ time WRITE setTime NOTIFY timeChanged)
		Q_PROPERTY (qreal _yPosition READ yPosition WRITE setYPosition NOTIFY yPositionChanged)
		Q_PROPERTY (QString _name READ name WRITE setName NOTIFY nameChanged)

	private:
		qreal _time, _yPosition, _width, _height;
		QString _name;
		/*
		std::list<TTTimeProcess*> _pluginsSmallView;
		std::list<TTTimeProcess*> _pluginsFullView;
		*/
		TimeEventModel* _pStartTimeEvent; /// The start TimeEvent of the timeProcess
		TimeEventModel* _pEndTimeEvent; /// The end TimeEvent of the timeProcess

	public:
		TimeboxModel (qreal t, 
					  qreal y, 
					  qreal w, 
					  qreal h, 
					  QString name, 
					  Timebox* pParent, 
					  TimeEventModel* pTimeEventStart, 
					  TimeEventModel* pTimeEventEnd);

	signals:
		void nameChanged (QString arg);
		void timeChanged (qreal arg);
		void timeEndChanged (qreal arg);
		void yPositionChanged (qreal arg);

	public slots:
		void setName (QString arg);
		void setTime (qreal arg);
		void setTimeEnd (qreal arg);
		void setYPosition (qreal arg);

	public:
		qreal time() const
		{
			return _time;
		}
		qreal yPosition() const
		{
			return _yPosition;
		}
		qreal width() const
		{
			return _width;
		}
		qreal height() const
		{
			return _height;
		}
		QString name() const
		{
			return _name;
		}
		
		TimeEventModel* startTimeEvent() const
		{
			return _pStartTimeEvent;
		}
		TimeEventModel* endTimeEvent() const
		{
			return _pEndTimeEvent;
		}

		/*
		const std::list<TTTimeProcess*>& pluginsSmallView() const
		{
			return _pluginsSmallView;
		}
		const std::list<TTTimeProcess*>& pluginsFullView()
		{
			if (_pluginsFullView.empty() )
			{
				_pluginsFullView.assign (_pluginsSmallView.begin(), _pluginsSmallView.end() );
			}

			return _pluginsFullView;
		}

		void addPluginSmall();
		void removePluginSmall();
		void addPluginFull();
		void removePluginFull();
*/
};

#endif // TIMEBOXMODEL_HPP
