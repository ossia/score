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

#ifndef TIMEBOXPRESENTER_HPP
#define TIMEBOXPRESENTER_HPP

class TimeboxModel;
class TimeboxSmallView;
class TimeboxFullView;
class TimeboxStorey;
class PluginView;
class QGraphicsView;
class QGraphicsScene;

enum ViewMode
{
  FULL,
  SMALL
};

#include <QObject>
#include <unordered_map>

class TimeboxPresenter : public QObject
{
  Q_OBJECT

private:
  QGraphicsView *_pView;
  TimeboxModel *_pModel;

  TimeboxSmallView *_pSmallView;
  QGraphicsScene *_pParentScene;
  TimeboxFullView *_pFullView;
  ViewMode _mode;

  std::unordered_map<TimeboxStorey*, PluginView*> _storeysSmallView;
  std::unordered_map<TimeboxStorey*, PluginView*> _storeysFullView;

public:
  TimeboxPresenter(TimeboxModel *pModel, TimeboxSmallView *pSmallView);

public slots:
  void storeyBarButtonClicked(bool id);

private:
private slots:
  void addStorey();
  void goFullView();
  void goSmallView();

public:
  void setView(QGraphicsView *pView) {_pView=pView;}
  const QGraphicsView* view() const {return _pView;}
  void setParentScene(QGraphicsScene *pScene) {_pParentScene=pScene;}

private:
  void createFullView();
  void deleteStorey(TimeboxStorey* tbs);

};

#endif // TIMEBOXPRESENTER_HPP
