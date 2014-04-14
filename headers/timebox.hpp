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
class QGraphicsRectItem;
class QString;

#include <QObject>
#include "utils.hpp"

class Timebox : public QObject
{
  Q_OBJECT

public:
  TimeboxPresenter *_pPresenter; /// @todo rendu public pour slot goSmallView()
  TimeboxSmallView *_pSmallView = nullptr;

  QGraphicsScene *_pScene; /// @todo Sert actuellement à héberger le scénario principal (qgraphicsscene) qui n'est pas encore un plugin Scenario (par jC)

private:
  TimeboxModel *_pModel;
  TimeboxFullView *_pFullView = nullptr;
  GraphicsView *_pGraphicsView;
  Timebox *_pParent = nullptr;
  static int staticId;

public:
  Timebox(Timebox *pParent = 0);
  explicit Timebox(Timebox *pParent, GraphicsView *pView, const QPointF &pos, float width, float height, ViewMode mode);
  explicit Timebox(Timebox *pParent, GraphicsView *pView, const QPointF &pos, float width, float height, ViewMode mode, QString name);
  ~Timebox();

signals:
  void isFull();
  void isSmall();
  void isHide();

private slots:
  void goFull();
  void goHide();
  void addChild (QGraphicsRectItem *rectItem);

public slots:
  void goSmall();

public:
  TimeboxModel* timeboxModel() const {return _pModel;} /// Used by GraphicsView's methods to retrieve width of the timebox
  void addChild (Timebox *other);
  void addChild (TimeEvent *timeEvent);

private:
  void init(const QPointF &pos, float height, float width, ViewMode mode, QString name);
};

#endif // TIMEBOX_HPP
