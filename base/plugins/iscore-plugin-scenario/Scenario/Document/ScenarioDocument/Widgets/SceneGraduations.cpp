#include <QObject>
#include <QPainter>
#include <algorithm>

#include "SceneGraduations.hpp"
#include <Process/Tools/ProcessGraphicsView.hpp>


class QWidget;

void SceneGraduations::setSize(const QSizeF& s)
{
  //prepareGeometryChange();
  m_size = s;
  update();
}

SceneGraduations::SceneGraduations(ProcessGraphicsView* view)
{
  QObject::connect(
      view, &ProcessGraphicsView::sizeChanged,
      [&](const QSize& s) { setSize(s); });
}

QRectF SceneGraduations::boundingRect() const
{
  return {0, 0, m_size.width(), m_size.height()};
}

void SceneGraduations::paint(
    QPainter* painter)
{
  painter->setPen(m_lines);
  painter->drawPath(m_grid);
}

void SceneGraduations::setGrid(QPainterPath&& newGrid)
{
  m_grid = std::move(newGrid);
  update();
}

void SceneGraduations::setColor(const QColor& col)
{
  m_lines = col;
  m_lines.setCosmetic(true);
  m_lines.setWidth(0);
}
