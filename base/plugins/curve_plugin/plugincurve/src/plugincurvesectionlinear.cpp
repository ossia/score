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

#include "plugincurvesectionlinear.hpp"
#include "plugincurvepoint.hpp"
#include "plugincurveview.hpp"
#include <QGraphicsItem>
#include <QPainter>
#include <QLineF>

PluginCurveSectionLinear::PluginCurveSectionLinear(QGraphicsObject *parent, PluginCurvePoint *source, PluginCurvePoint *dest) :
  PluginCurveSection(parent,source,dest)
{
  _coef = 0;
}

QRectF PluginCurveSectionLinear::boundingRect() const
{
    QPointF source = QPointF(0,0);
    QPointF dest = (_pDestPoint->scenePos() - _pSourcePoint->scenePos());
    return QRectF(qMin(dest.x(),source.x()) - SHAPEHEIGHT,
                  qMin(dest.y(),source.y()) - SHAPEHEIGHT,
                  qAbs(dest.x()-source.x()) + 2*SHAPEHEIGHT,
                  qAbs(dest.y()-source.y()) + 2*SHAPEHEIGHT);
}

QPainterPath PluginCurveSectionLinear::shape() const
{
  QPointF source = QPointF(0,0);
  QPointF dest = (_pDestPoint->scenePos() - _pSourcePoint->scenePos());
  QLineF line = QLineF(source,dest);
  qreal length = line.length();
  QPointF vector = QPointF(line.dx()*SHAPEHEIGHT/length,line.dy()*SHAPEHEIGHT/length);
  QPointF tVector = QPointF(-vector.y(),vector.x());

  QPainterPath painterPath = QPainterPath(source);
  if (length > 2 * SHAPEHEIGHT) // if enough length
    {
      painterPath.lineTo(source + tVector + vector);
      painterPath.lineTo(dest - vector + tVector);
    }
  painterPath.lineTo(dest);
  if (length > 2 * SHAPEHEIGHT) // if enough length
    {
      painterPath.lineTo(dest - vector - tVector);
      painterPath.lineTo(source + vector - tVector);
    }
  painterPath.closeSubpath();
  return painterPath;
}

QPainterPath PluginCurveSectionLinear::path() const
{
  QPointF source = _pSourcePoint->pos();
  QPointF dest = _pDestPoint->pos();
  QPainterPath path = QPainterPath(QPointF(0.0,0.0));
  path.lineTo(mapToScene(dest)-mapToScene(source));
  return path;
}

void PluginCurveSectionLinear::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option)
    Q_UNUSED(widget)
    QPen pen(QColor(Qt::darkGray).light(30), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPointF source = QPointF(0,0);
    QPointF dest = (_pDestPoint->scenePos() - _pSourcePoint->scenePos());
    QLineF line = QLineF(source,dest);
    qreal length = line.length();
    // ---> set gradients
    QPointF vector = QPointF(line.dx()*SHAPEHEIGHT/length,line.dy()*SHAPEHEIGHT/length);
    QPointF tVector = QPointF(-vector.y(),vector.x());
    QLinearGradient gradientExt = QLinearGradient(-source,tVector);
    QLinearGradient gradientNoSelection = QLinearGradient(-source,tVector);
    gradientExt.setSpread(QGradient::ReflectSpread);
    gradientExt.setColorAt(0,selectColor().light(140));
    gradientExt.setColorAt(0.6,Qt::transparent);
    gradientNoSelection.setSpread(QGradient::ReflectSpread);
    gradientNoSelection.setColorAt(0,color());
    gradientNoSelection.setColorAt(0.3,Qt::transparent);
    // <--- set the gradient
    if (qFuzzyCompare(line.length(), qreal(0.))) // If line too short, do nothing
        return;
    /// @todo trouver un moyen de faire des belles lignes sans gradient et en utilisant path()
    // Draw the line
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setPen(pen);
    painter->drawPath(path());
    // No pen needed now
    painter->setPen(Qt::NoPen);
    // draw gradient
    painter->setBrush(gradientNoSelection);
    painter->drawPath(shape());
    // Draw the hilight
    if (_highlight)
      {
        painter->setBrush(gradientExt);
        painter->drawPath(shape());
      }
}
