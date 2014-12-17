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

#ifndef PLUGINCURVE_HPP
#define PLUGINCURVE_HPP

#include <QObject>
#include <QGraphicsItem>
class PluginCurveModel;
class PluginCurveView;
class PluginCurvePresenter;

class PluginCurve : public QObject
{
  Q_OBJECT

    /*!
    *  This class is the interface of the curve plugin.
    *  It emits signals after user's interactions with the elements of the plugin. @n
    *
    *  @brief Grid
    *  @author Simon Touchard, Myriam Desainte-Catherine
    *  @date 2014
    */

// Attributes
private:
  PluginCurveModel *_pModel; /*!< Curve plugin model.*/
  PluginCurvePresenter *_pPresenter; /*!< Curve plugin presenter. */
  PluginCurveView *_pView; /*!< Curve plugin view. */

// Signals and slots
signals :
  // Signals for the plugin users (va dans le présenteur)
  void notifyPointCreated(QPointF value); /*!< Notifies the user that a point has been created. */
  void notifyPointDeleted(QPointF value); /*!< Notifies the user that a point has been deleted. */
  void notifyPointMoved(QPointF oldVal, QPointF newVal); /*!< Notifies the user that a point has been moved.*/
  void notifySectionCreated(QPointF source, QPointF dest, qreal coef); /*!< Notifies the user that a section has been created. */
  void notifySectionChanged(QPointF source, QPointF dest, qreal coef); /*!< Notifies the user that a section has been changed. */
  void notifySectionDeleted(QPointF source, QPointF dest); /*!< Notifies the user that a section has been deleted. */
  void notifySectionMoved(QPointF oldSource, QPointF oldDest, QPointF newSource, QPointF newDest);

public slots :

// Methods
public:
  //! Construct a PluginCurve.
  /*!
  \param parent Parent item. Should be large enough.
  */
  PluginCurve(QGraphicsObject *parent);
  ~PluginCurve();
  /*! Activates area selection mode. Deactivates the others modes. If area selection mode is activated, nothing is done. */
  void setAreaSelectionMode();
  /*! Hides the grid if b is false. Show the grid if b is true. */
  void setGridVisible(bool b = true);
  /*! Activates linear selection mode. Deactivates the others modes. If linear selection mode is activated, nothing is done. */
  void setLinearSelectionMode();
  /*! Activates (b is true) or deactivates (b is false) grid magnetism. */
  void setMagnetism(bool b = true);
  /*! Activates pen mode. Deactivates the others modes. If pen mode is already activated, nothing is done.*/
  void setPenMode();
  /*! Allows (b is true) or forbids (b is false) points to cross others points. */
  void setPointCanCross(bool b = true);
  /*! Returns the view. */
  QGraphicsObject *view();
//  /*! Create point*/
//  void createPoint(QPointF pos);
};

#endif // PLUGINCURVE_HPP
