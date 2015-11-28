#pragma once
#include <QGraphicsItem>
#include <QPen>
class ScenarioBaseGraphicsView;
class SceneGraduations final : public QGraphicsItem
{
    private:
        QPen m_lines;
        QPainterPath m_grid;
        QSizeF m_size;
        void setSize(const QSizeF& s);

    public:
        SceneGraduations(ScenarioBaseGraphicsView* view);

        QRectF boundingRect() const override;
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

        void setGrid(QPainterPath&& newGrid);
        void setColor(const QColor& col);
};
