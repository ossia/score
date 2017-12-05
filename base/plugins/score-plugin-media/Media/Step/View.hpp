#pragma once
#include <Process/LayerView.hpp>
#include <Media/Step/Model.hpp>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>

namespace Media
{
namespace Step
{

class View final : public Process::LayerView
{
    Q_OBJECT
  public:
    explicit View(QGraphicsItem* parent)
      : Process::LayerView{parent}
    {
    }

    const Model* m_model{};

  signals:
    void pressed();
    void askContextMenu(const QPoint & t1, const QPointF & t2);

    void change(int, float);
  private:
    void paint_impl(QPainter* p) const override
    {
      if(!m_model)
        return;

      p->setPen(Qt::red);
      auto h = boundingRect().height();
      auto bar_w = 20.;

      const auto& steps = m_model->steps();
      for(std::size_t i = 0; i < steps.size(); i++)
      {
        p->fillRect(QRectF{i * bar_w, steps[i] * h, bar_w, h - steps[i] * h}, QColor(Qt::red).lighter());
        p->drawLine(QPointF{i * bar_w, steps[i] * h}, QPointF{(i + 1) * bar_w, steps[i] * h});
      }

    }
    void mousePressEvent(QGraphicsSceneMouseEvent* ev) override
    {
      if(!m_model)
        return;

      ev->accept();
      int pos = ev->pos().x() / 20;
      if(pos >= 0 && pos < m_model->steps().size())
      {
        emit change(pos, ev->pos().y() / boundingRect().height());
      }
    }

    void mouseMoveEvent(QGraphicsSceneMouseEvent* ev) override
    {
      ev->accept();
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev) override
    {
      ev->accept();
    }

    void contextMenuEvent(QGraphicsSceneContextMenuEvent* ev) override
    {
      ev->accept();
    }

};

}
}
