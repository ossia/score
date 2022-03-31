#pragma once
#include <avnd/concepts/painter.hpp>
#include <QPainter>
#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>

namespace oscr
{
struct QPainterAdapter
{
    QPainter& painter;
    QGraphicsItem& item;
    QPainterPath path;

    void begin_path()
    {
        path = QPainterPath{};
    }
    void close_path()
    {
        path.closeSubpath();
    }
    void stroke()
    {
        painter.strokePath(path, painter.pen());
    }
    void fill()
    {
        painter.fillPath(path, painter.brush());
    }
    void update()
    {
        item.update();
    }

    void move_to(double x, double y)
    {
        path.moveTo(x, y);
    }
    void line_to(double x, double y)
    {
        path.lineTo(x, y);
    }
    void arc_to(double x, double y, double w, double h, double startAngle, double arcLength)
    {
        path.arcTo(x, y, w, h, startAngle, arcLength);
    }


    void cubic_to(double c1x, double c1y, double c2x, double c2y, double endx, double endy)
    {
        path.cubicTo(c1x, c1y, c2x, c2y, endx, endy);
    }
    void quad_to(double x1, double y1, double x2, double y2)
    {
        path.quadTo(x1, y1, x2, y2);
    }

    void translate(double x, double y)
    {
        painter.translate(x, y);
    }
    void scale(double x, double y)
    {
        painter.scale(x, y);
    }
    void rotate(double a)
    {
        painter.rotate(a);
    }
    void reset_transform()
    {
        painter.resetTransform();
    }

    // Colors:
    //                  R    G    B    A
    void set_stroke_color(QColor c)
    {
        QPen p = painter.pen();
        p.setColor(c);
        painter.setPen(p);
    }
    void set_stroke_width(double w)
    {
        QPen p = painter.pen();
        p.setWidth(w);
        painter.setPen(p);
    }

    void set_fill_color(QColor b)
    {
        painter.setBrush(b);
    }

    // Text:
    void set_font(QString f)
    {
        auto font = painter.font();
        font.setFamily(f);
        painter.setFont(font);
    }

    void set_font_size(double f)
    {
        auto font = painter.font();
        font.setPointSize(f);
        painter.setFont(font);
    }

    void draw_text(double x, double y, QString str)
    {
        path.addText(x, y, painter.font(), str);
    }

    // Drawing
    //          x1, y1, x2 , y2
    void draw_line(double x1, double y1, double x2, double y2)
    {
        path.moveTo(x1, y1);
        path.lineTo(x2, y2);
    }

    //          x , y , w  , h
    void draw_rect(double x, double y, double w, double h)
    {
        path.addRect(x, y, w, h);
    }

    //                  x , y , w  , h
    void draw_rounded_rect(double x, double y, double w, double h, double r)
    {
        path.addRoundedRect(x, y, w, h, r, r);
    }

    //            x , y , filename
    void draw_pixmap(double x, double y, QString str)
    {
        painter.drawPixmap(x, y, str);
    }

    //             x , y , w  , h
    void draw_ellipse(double x, double y, double w, double h)
    {
        path.addEllipse(x, y, w, h);
    }

    //            cx, cy, radius
    void draw_circle(double cx, double cy, double cr)
    {
        path.addEllipse(QPointF{cx, cy}, cr, cr);
    }
};
static_assert(avnd::painter<QPainterAdapter>);

template<typename Item, typename Control = void>
class CustomItem : public QGraphicsItem
{
  public:
    CustomItem()
    {
      this->setFlag(ItemClipsToShape);
      this->setFlag(ItemClipsChildrenToShape);
      if constexpr(requires { impl.transaction; }) {
        impl.transaction.start = [] {};
        impl.transaction.update = [this] (const auto& value) {
          impl.value = value; //impl.value_to_control(Control{}
          update();
        };
        impl.transaction.commit = [] {};
        impl.transaction.rollback = [] {};
      }
    }

    QRectF boundingRect() const override
    {
      return {0., 0., Item::width(), Item::height()};
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override
    {
      painter->setRenderHint(QPainter::Antialiasing, true);
      impl.paint(QPainterAdapter{*painter, *this, {}});
      painter->setRenderHint(QPainter::Antialiasing, false);
    }

  protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override
    {
      if(impl.mouse_press(event->pos().x(), event->pos().y()))
        event->accept();
    }
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override
    {
      impl.mouse_move(event->pos().x(), event->pos().y());
        event->accept();
    }
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override
    {
      impl.mouse_release(event->pos().x(), event->pos().y());
      event->accept();
    }

  private:
    Item impl;
};
}
