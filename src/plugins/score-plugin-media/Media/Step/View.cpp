#include <Process/ProcessContext.hpp>
#include <score/model/Skin.hpp>

#include <Automation/AutomationColors.hpp>
#include <Media/Step/Model.hpp>
#include <Media/Step/View.hpp>

#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>

#include <wobjectimpl.h>

W_OBJECT_IMPL(Media::Step::View)
W_OBJECT_IMPL(Media::Step::Item)
namespace Media::Step
{
//! One step, drawn like a box of the pattern sequencer: the bar in the note
//! colour, the space above it in the rest colour, both lit while the step
//! plays. The repetitions the layer draws past the end of the sequence are the
//! darker note colour only.
static void paintStep(QPainter* p, QRectF r, float step, bool playing, bool repeat)
{
  auto& skin = score::Skin::instance();
  const QRectF rest{r.x(), r.y(), r.width(), step * r.height()};
  const QRectF bar{r.x(), rest.bottom(), r.width(), r.height() - rest.height()};
  if(!repeat)
    p->fillRect(rest, playing ? skin.Emphasis2.lighter.brush : skin.Emphasis2.main.brush);
  p->fillRect(
      bar, repeat    ? skin.Base4.darker.brush
           : playing ? skin.Base4.lighter.brush
                     : skin.Base4.main.brush);
}

//! Room between two steps, as between the pattern sequencer's boxes.
static QRectF stepRect(double x, double w, double h)
{
  const double gap = w > 4. ? 1. : 0.;
  return {x, 0., w - gap, h};
}

View::View(const Model& model, QGraphicsItem* parent)
    : Process::LayerView{parent}
    , m_model{model}
{
  setFlag(QGraphicsItem::ItemClipsToShape);
  con(model, &Step::Model::execPosition, this, [this](int s) {
    if(s != m_playing)
    {
      m_playing = s;
      update();
    }
  });
}

View::~View() = default;

void View::setBarWidth(double v)
{
  m_barWidth = v;
  update();
}

void View::paint_impl(QPainter* p) const
{
  if(m_barWidth > 2.)
  {
    const auto h = boundingRect().height();
    const auto w = boundingRect().width();
    const auto bar_w = m_barWidth;
    const auto& steps = m_model.steps();

    // The sequence, then its repetitions to the end of the layer.
    std::size_t i = 0;
    for(double x = 0.; x < w; x += bar_w, i++)
    {
      const auto idx = i % steps.size();
      const bool repeat = i >= steps.size();
      paintStep(
          p, stepRect(x, bar_w, h), steps[idx], !repeat && int(idx) == m_playing,
          repeat);
    }
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
  if(pos < m_model.steps().size())
  {
    change(pos, ev->pos().y() / boundingRect().height());
  }
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();

  std::size_t pos = std::size_t(ev->pos().x() / m_barWidth) % m_model.steps().size();
  if(pos < m_model.steps().size())
  {
    change(pos, ev->pos().y() / boundingRect().height());
  }
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  released(ev->pos());
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
  con(m, &Step::Model::currentSequenceChanged, this, [&] { update(); });
  con(m, &Step::Model::execPosition, this, [this](int s) {
    if(s != m_playing)
    {
      m_playing = s;
      update();
    }
  });
}

Item::~Item() = default;

void Item::paint(QPainter* p, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  const auto h = boundingRect().height();
  const auto w = boundingRect().width();
  const auto& steps = m_model.steps();
  const auto bar_w = w / steps.size();

  for(std::size_t i = 0; i < steps.size(); i++)
    paintStep(p, stepRect(i * bar_w, bar_w, h), steps[i], int(i) == m_playing, false);
}

void Item::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  std::size_t pos
      = qBound(0., ev->pos().x() / boundingRect().width(), 1.) * m_model.steps().size();
  updateSteps(m_model, m_disp, pos, ev->pos().y() / boundingRect().height());
}

void Item::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();

  std::size_t pos
      = qBound(0., ev->pos().x() / boundingRect().width(), 1.) * m_model.steps().size();
  updateSteps(m_model, m_disp, pos, ev->pos().y() / boundingRect().height());
}

void Item::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  m_disp.commit();
}

void updateSteps(
    const Model& m, SingleOngoingCommandDispatcher<ChangeSteps>& disp, std::size_t num,
    float v)
{
  if(num < m.steps().size())
  {
    auto vec = m.steps();
    vec[num] = ossia::clamp(v, 0.f, 1.f);
    disp.submit(m, std::move(vec));
  }
}

}
