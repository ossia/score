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

#ifndef TIMEEVENTMODEL_HPP
#define TIMEEVENTMODEL_HPP

#include <QObject>
#include <QString>
#include <QVector>

class Timebox;

/*!
 *  The model is linked with OSSIA API, and permits to maintain all elements used later by
 *  the other classes of TimeEvent.
 *
 *  @brief Maintain the model of a TimeEvent, no graphics here.
 *  @author Jaime Chao
 *  @date 2014
*/

class TimeEventModel : public QObject
{
		Q_OBJECT

		Q_PROPERTY (int index READ index)
		Q_PROPERTY (qreal m_time READ time WRITE setTime NOTIFY timeChanged)
		Q_PROPERTY (qreal m_yPosition READ yPosition 
									  WRITE setYPosition 
									  NOTIFY yPositionChanged)
		Q_PROPERTY (QString m_name READ name WRITE setName NOTIFY nameChanged)

	public:
		TimeEventModel (int id, qreal t, qreal y, QString name, QObject* parent);
		
		qreal time() const
		{
			return m_time;
		}
		
		qreal yPosition() const
		{
			return m_yPosition;
		}
		
		QString name() const
		{
			return m_name;
		}

		void addTimebox (Timebox* tb);
		
		int index() const
		{
			return m_index;
		}
		
		static const int nextId() ;
		
	signals:
		void nameChanged (QString arg);
		void timeChanged (qreal arg);
		void yPositionChanged (qreal arg);

	public slots:
		void setName (QString arg);
		void setTime (qreal arg);
		void setYPosition (qreal arg);
		
	private: 
		qreal m_time{};
		qreal m_yPosition{};
		QString m_name;

		QVector<Timebox*> _TimeBoxes; /// Vector containing all the Timebox linked to this TimeEvent
		
		int m_index;
		
		static int m_staticId;
};

#endif // TIMEEVENTMODEL_HPP
