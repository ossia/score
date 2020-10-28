#include <Media/Step/View.hpp>
#include <Automation/AutomationColors.hpp>
#include <Media/Step/Model.hpp>
#include <score/tools/Bind.hpp>
#include <Process/ProcessContext.hpp>

W_OBJECT_IMPL(Media::Step::View)
W_OBJECT_IMPL(Media::Step::Item)
namespace Media::Step
{

View::View(const Model& model, QGraphicsItem* parent)
  : Process::LayerView{parent}
  , m_model{model}
{
  setFlag(QGraphicsItem::ItemClipsToShape);
}

View::~View() = default;

void View::setBarWidth(double v)
{
  m_barWidth = v;
  update();
}

void View::paint_impl(QPainter* p) const
{
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

    const auto& steps = m_model.steps();
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

void View::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  pressed(ev->pos());
  std::size_t pos = std::size_t(ev->pos().x() / m_barWidth) % m_model.steps().size();
  if (pos < m_model.steps().size())
  {
    change(pos, ev->pos().y() / boundingRect().height());
  }
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();

  std::size_t pos = std::size_t(ev->pos().x() / m_barWidth) % m_model.steps().size();
  if (pos < m_model.steps().size())
  {
    change(pos, ev->pos().y() / boundingRect().height());
  }
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  released(ev->pos());
}

void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  askContextMenu(ev->screenPos(), ev->scenePos());
  ev->accept();
}



Item::Item(const Model& m, const Process::Context& ctx, QGraphicsItem* parent)
  : score::EmptyRectItem{parent}
  , m_model{m}
  , m_disp{ctx.commandStack}
{
  setAcceptedMouseButtons(Qt::LeftButton);
  setRect({0, 0, 300, 120});
  setFlag(QGraphicsItem::ItemClipsToShape);
  setFlag(QGraphicsItem::ItemHasNoContents, false);
  con(m, &Step::Model::stepsChanged, this, [&] { update(); });
  con(m, &Step::Model::stepCountChanged, this, [&] { update(); });
}

Item::~Item() = default;

void Item::paint(QPainter* p, const QStyleOptionGraphicsItem* option, QWidget* widget)
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
  auto cur_pos = 0.;

  const auto& steps = m_model.steps();
  const auto bar_w = w / steps.size();

  for(auto& step : steps)
  {
    p->fillRect(QRectF{cur_pos, step * h, (float)bar_w, h - step * h}, br);
    p->drawLine(QPointF{cur_pos, step * h}, QPointF{cur_pos + bar_w, step * h});

    cur_pos += bar_w;
  }
  p->setRenderHint(QPainter::Antialiasing, false);
}

void Item::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  std::size_t pos = qBound(0., ev->pos().x() / boundingRect().width(), 1.) * m_model.steps().size();
  updateSteps(m_model, m_disp, pos, ev->pos().y() / boundingRect().height());
}

void Item::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();

  std::size_t pos = qBound(0., ev->pos().x() / boundingRect().width(), 1.) * m_model.steps().size();
  updateSteps(m_model, m_disp, pos, ev->pos().y() / boundingRect().height());
}

void Item::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  m_disp.commit();
}

void Item::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  /*
  askContextMenu(ev->screenPos(), ev->scenePos());
  ev->accept();
  */
}

void updateSteps(
      const Model& m,
      SingleOngoingCommandDispatcher<ChangeSteps>& disp, std::size_t num, float v)
{
  auto vec = m.steps();
  if (num > vec.size())
  {
    vec.resize(num, 0.5f);
  }
  v = ossia::clamp(v, 0.f, 1.f);
  vec[num] = v;

  disp.submit(m, std::move(vec));
}

}
