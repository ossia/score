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

#ifndef TIMEBOXSMALLVIEW_HPP
#define TIMEBOXSMALLVIEW_HPP

class TimeboxHeader;
class TimeboxStorey;
class TimeboxModel;
class QGraphicsLinearLayout;
class Timebox;

#include "utils.hpp"
#include <QGraphicsWidget>

/*!
 *  This class is the graphical representation of a child Timebox.
 *  This class manages a TimeboxHeader and the storeys with the Qt layout system.
 *
 *  @brief Timebox small view
 *  @author Jaime Chao, Clément Bossut
 *  @date 2013/2014
 */

class TimeboxSmallView : public QGraphicsWidget
{
  Q_OBJECT

private:
  TimeboxModel *_pModel;
  TimeboxHeader *_pHeader;

  QGraphicsLinearLayout *_pLayout;

public:
  TimeboxSmallView(TimeboxModel *pModel, Timebox *parentObject, QGraphicsItem *parentGraphics = 0);

signals:
  void headerDoubleClicked();
  void suppressTimebox();
  void xChanged(qreal);
  void yChanged(qreal);

public slots:
  void setY(qreal);
  void setX(qreal);

public:
  enum {Type = BoxItemType}; //! Type value for custom item. Enable the use of qgraphicsitem_cast with this item
  virtual int type() const {return Type;}

  void addStorey(TimeboxStorey *pStorey);
  TimeboxModel* model() const {return _pModel;}

  // QGraphicsItem interface
  virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);
  virtual QRectF boundingRect() const;

protected:
  // QGraphicsItem interface
  void keyPressEvent(QKeyEvent *event);
  QVariant itemChange(GraphicsItemChange change, const QVariant &value);
};

#endif // TIMEBOXSMALLVIEW_HPP
