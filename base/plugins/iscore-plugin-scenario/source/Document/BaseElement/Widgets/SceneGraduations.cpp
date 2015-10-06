#include "SceneGraduations.hpp"
#include "ScenarioBaseGraphicsView.hpp"
#include <QObject>
#include <QDebug>

void SceneGraduations::setSize(const QSizeF& s)
{
    prepareGeometryChange();
    m_size = s;
    update();
}


SceneGraduations::SceneGraduations(ScenarioBaseGraphicsView* view)
{
    QObject::connect(view, &ScenarioBaseGraphicsView::sizeChanged,
                     [&] (const QSize& s) { setSize(s); });
}


QRectF SceneGraduations::boundingRect() const
{
    return {0, 0, m_size.width(), m_size.height()};
}


void SceneGraduations::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
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
