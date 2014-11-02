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

class TimeEvent;
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

  Q_PROPERTY(qreal _time READ time WRITE settime NOTIFY timeChanged)
  Q_PROPERTY(qreal _yPosition READ yPosition WRITE setYPosition NOTIFY yPositionChanged)
  Q_PROPERTY(QString _name READ name WRITE setname NOTIFY nameChanged)

private:
  qreal _time, _yPosition;
  QString _name;

  QVector<Timebox*> _timeboxes; /// Vector containing all the Timebox linked to this TimeEvent

public:
  TimeEventModel(qreal t, qreal y, QString name, TimeEvent *parent);

signals:
  void nameChanged(QString arg);
  void timeChanged(qreal arg);
  void yPositionChanged(qreal arg);

public slots:
  void setname(QString arg);
  void settime(qreal arg);
  void setYPosition(qreal arg);

public:
  qreal time() const {return _time;}
  qreal yPosition() const {return _yPosition;}
  QString name() const {return _name;}

  void addTimebox(Timebox *tb);
};

#endif // TIMEEVENTMODEL_HPP
