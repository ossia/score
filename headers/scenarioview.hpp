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

#ifndef SCENARIOVIEW_HPP
#define SCENARIOVIEW_HPP

class QGraphicsRectItem;

#include "pluginview.hpp"
#include <QPointF>

/*!
 *  View of the Scenario plugin, inherits the class PluginView.
 *  Permits to draw a timebox in smallView, and add it as child to the current one in fullView. @n
 *  NOTE : in ScoreAPI the hierarchy is managed by Scenario, in i-score it is managed by Timebox.
 *
 *  @brief View of the Scenario plugin
 *  @author Jaime Chao
 *  @date 2013/2014
 */
class ScenarioView : public PluginView
{
  Q_OBJECT

private:
  QGraphicsRectItem *_pTemporaryBox = nullptr;  /// Temporary graphical box when a creation is in progress.
  QPointF _pressPoint;                          /// Last pression point.
  QPointF _releasePoint;                        /// Last release point.

  static const int MIN_BOX_WIDTH = 100;          /// The minimal width constraint to the new timebox being added
  static const int MIN_BOX_HEIGHT = 50;

public:
  ScenarioView(QGraphicsItem *parent = 0);

signals:
  void addTimebox(QGraphicsRectItem *rectItem);  /// Add a child timebox to the current one

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent *mouseEvent);
  void mouseMoveEvent(QGraphicsSceneMouseEvent *mouseEvent);
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *mouseEvent);
};

#endif // SCENARIOVIEW_HPP
