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

#ifndef GRAPHICSTIMEEVENT_HPP
#define GRAPHICSTIMEEVENT_HPP

class TimeEventModel;
class TimeEventPresenter;
class TimeEventView;
class Timebox;
class GraphicsView;

#include <QObject>
#include <QLineF>
#include"utils.hpp"

/*!
 *  This class maintains together all the classes needed by a TimeEvent, offering a placeholder and makes interaction easier with TimeEvent object in i-score. @n
 *
 *  @brief TimeEvent Interface
 *  @author Jaime Chao
 *  @date 2014
 */

class TimeEvent : public QObject
{
  Q_OBJECT

private:
  TimeEventModel *_pModel = nullptr;
  TimeEventPresenter *_pPresenter = nullptr;
  TimeEventView *_pView = nullptr;

  static int staticId; /// Give a unique number to each instance of TimeEvent

public:
  TimeEvent(Timebox *pParent, const QPointF &pos);
  ~TimeEvent();

signals:
  void createTimeEventAndTimeboxProxy(QLineF line);  /// Proxy signal for TimeBox creation from TimeEvent view to Timebox parent

public:
  TimeEventView* view() const {return _pView;}
  TimeEventModel* model() const {return _pModel;}
};

#endif // GRAPHICSTIMEEVENT_HPP
