#pragma once
#include <Automation/AutomationColors.hpp>
#include <Media/Step/Model.hpp>
#include <Process/LayerView.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QPainter>

#include <verdigris>

namespace Media
{
namespace Step
{

class View final : public Process::LayerView
{
  W_OBJECT(View)
public:
  explicit View(QGraphicsItem* parent) : Process::LayerView{parent}
  {
    setFlag(QGraphicsItem::ItemClipsToShape);
  }

  void setBarWidth(double v)
  {
    m_barWidth = v;
    update();
  }
  const Model* m_model{};

public:
  void change(int arg_1, float arg_2) W_SIGNAL(change, arg_1, arg_2);

private:
  void paint_impl(QPainter* p) const override
  {
    if (!m_model)
      return;

    if (m_barWidth > 2.)
    {
      p->setRenderHint(QPainter::Antialiasing, true);

      static QPen pen(QColor{"#ff9900"});
      pen.setWidth(2.);
      static QBrush br{QColor{"#ffad33"}};
      static QPen pen2{QColor{"#ffb84d"}};
      pen.setWidth(2.);
      static QBrush br2{QColor{"#ffcc80"}};
      p->setPen(pen);

      const auto h = boundingRect().height();
      const auto w = boundingRect().width();
      const auto bar_w = m_barWidth;
      auto cur_pos = 0.;

      const auto& steps = m_model->steps();
      std::size_t i = 0;
      while (cur_pos < w)
      {
        auto idx = i % steps.size();
        auto step = steps[idx];
        p->fillRect(QRectF{cur_pos, step * h, (float)bar_w, h - step * h}, br);
        p->drawLine(QPointF{cur_pos, step * h}, QPointF{cur_pos + bar_w, step * h});

        cur_pos += bar_w;
        i++;
        if (i == steps.size())
        {
          break;
        }
      }

      // Now draw the echo
      p->setPen(pen2);
      while (cur_pos < w)
      {
        auto idx = i % steps.size();
        auto step = steps[idx];
        p->fillRect(QRectF{cur_pos, step * h, (float)bar_w, h - step * h}, br2);
        p->drawLine(QPointF{cur_pos, step * h}, QPointF{cur_pos + bar_w, step * h});

        cur_pos += bar_w;
        i++;
      }

      p->setRenderHint(QPainter::Antialiasing, false);
    }
    else
    {
      p->drawText(boundingRect(), Qt::AlignCenter, tr("Zoom in to edit steps"));
    }
  }

  void mousePressEvent(QGraphicsSceneMouseEvent* ev) override
  {
    if (!m_model)
      return;

    ev->accept();
    pressed(ev->pos());
    std::size_t pos = std::size_t(ev->pos().x() / m_barWidth) % m_model->steps().size();
    if (pos < m_model->steps().size())
    {
      change(pos, ev->pos().y() / boundingRect().height());
    }
  }

  void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override
  {
    if (!m_model)
      return;

    ev->accept();

    std::size_t pos = std::size_t(ev->pos().x() / m_barWidth) % m_model->steps().size();
    if (pos < m_model->steps().size())
    {
      change(pos, ev->pos().y() / boundingRect().height());
    }
  }

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override
  {
    ev->accept();
    released(ev->pos());
  }

  void contextMenuEvent(QGraphicsSceneContextMenuEvent* ev) override
  {
    askContextMenu(ev->screenPos(), ev->scenePos());
    ev->accept();
  }

private:
  double m_barWidth{};
};
}
}
