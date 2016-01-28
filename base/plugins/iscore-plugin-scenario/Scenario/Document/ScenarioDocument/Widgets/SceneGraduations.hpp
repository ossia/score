#pragma once
#include <QColor>
#include <QGraphicsItem>
#include <QPainterPath>
#include <QPen>
#include <QRect>
#include <QSize>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;
class ProcessGraphicsView;

class SceneGraduations final : public QGraphicsItem
{
    private:
        QPen m_lines;
        QPainterPath m_grid;
        QSizeF m_size;
        void setSize(const QSizeF& s);

    public:
        SceneGraduations(ProcessGraphicsView* view);

        QRectF boundingRect() const override;
        void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

        void setGrid(QPainterPath&& newGrid);
        void setColor(const QColor& col);
};
