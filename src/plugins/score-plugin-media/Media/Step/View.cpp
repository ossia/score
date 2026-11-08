#include <Process/ProcessContext.hpp>
#include <score/model/Skin.hpp>

#include <Automation/AutomationColors.hpp>
#include <Media/Step/Model.hpp>
#include <Media/Step/View.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/tools/Bind.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QTimer>

#include <numeric>
#include <random>

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

int View::stepAt(double x) const noexcept
{
  const auto n = m_model.steps().size();
  if(m_barWidth <= 2. || n == 0)
    return -1;
  // The repetitions past the end of the sequence edit the step they repeat.
  return int(std::size_t(std::max(0., x) / m_barWidth) % n);
}

void View::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  pressed(ev->pos());
  if(ev->button() != Qt::LeftButton)
    return;
  m_editing = true;
  if(auto v = m_gesture.press(
         m_model, stepAt(ev->pos().x()), ev->pos(), ev->modifiers(),
         boundingRect().height()))
    stepsEdited(*v);
}

void View::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  if(!m_editing)
    return;
  if(auto v = m_gesture.move(
         m_model, stepAt(ev->pos().x()), ev->pos(), boundingRect().height()))
    stepsEdited(*v);
}

void View::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  endGesture();
}

void View::endGesture()
{
  if(!m_editing)
    return;
  m_editing = false;
  m_gesture.release();
  released({});
}

//! A button-less move makes the scene drop the grab without any release: the
//! edit has to end there too, or the next press would extend it.
bool View::sceneEvent(QEvent* ev)
{
  if(ev->type() == QEvent::UngrabMouse)
    endGesture();
  return LayerView::sceneEvent(ev);
}

void View::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  // The press asked for focus through a queued connection, and the menu only
  // opens on the focused layer: ask once the focus is through. The press also
  // grabbed the mouse, which the menu needs to highlight what it hovers.
  ev->accept();
  endGesture();
  if(scene() && scene()->mouseGrabberItem() == this)
    ungrabMouse();
  QTimer::singleShot(0, this, [this, screen = ev->screenPos(), sp = ev->scenePos()] {
    askContextMenu(screen, sp);
  });
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

int Item::stepAt(double x) const noexcept
{
  const auto n = m_model.steps().size();
  const double w = boundingRect().width();
  if(n == 0 || w <= 0.)
    return -1;
  return std::min(int(n) - 1, int(std::clamp(x / w, 0., 1.) * n));
}

void Item::mousePressEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  if(auto v = m_gesture.press(
         m_model, stepAt(ev->pos().x()), ev->pos(), ev->modifiers(),
         boundingRect().height()))
    m_disp.submit(m_model, std::move(*v));
}

void Item::mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  if(auto v = m_gesture.move(
         m_model, stepAt(ev->pos().x()), ev->pos(), boundingRect().height()))
    m_disp.submit(m_model, std::move(*v));
}

void Item::mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
{
  ev->accept();
  endGesture();
}

void Item::endGesture()
{
  m_gesture.release();
  m_disp.commit();
}

bool Item::sceneEvent(QEvent* ev)
{
  if(ev->type() == QEvent::UngrabMouse)
    endGesture();
  return score::EmptyRectItem::sceneEvent(ev);
}

void Item::contextMenuEvent(QGraphicsSceneContextMenuEvent* ev)
{
  ev->accept();
  endGesture();
  QMenu menu;
  fillStepsMenu(menu, m_model, m_disp.stack());
  menu.exec(ev->screenPos());
}

std::optional<ossia::float_vector> StepsGesture::press(
    const Model& m, int step, QPointF pos, Qt::KeyboardModifiers mods, double height)
{
  release();
  if(height <= 0.)
    return std::nullopt;
  if(mods & (Qt::ShiftModifier | Qt::AltModifier))
  {
    const auto& cur = m.steps();
    std::vector<float> base(cur.begin(), cur.end());
    std::vector<float> target(base.size());
    std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> dist{0.f, 1.f};
    for(auto& v : target)
      v = dist(rng);
    std::vector<int> steps(base.size());
    std::iota(steps.begin(), steps.end(), 0);
    m_drag.start(
        (mods & Qt::ShiftModifier) ? score::BarsDrag::Kind::Offset
                                   : score::BarsDrag::Kind::Random,
        pos, std::move(base), std::move(target), std::move(steps));
    // Nothing moves until the mouse does.
    return std::nullopt;
  }
  m_setting = true;
  return move(m, step, pos, height);
}

std::optional<ossia::float_vector>
StepsGesture::move(const Model& m, int step, QPointF pos, double height)
{
  if(height <= 0.)
    return std::nullopt;
  if(m_drag)
  {
    // Stored as drawn: 1 at the bottom, 0 at the top.
    const auto v = m_drag.values(pos, height, 1.f, 0.f);
    const auto& cur = m.steps();
    if(v.size() != cur.size() || std::equal(v.begin(), v.end(), cur.begin()))
      return std::nullopt;
    return ossia::float_vector(v.begin(), v.end());
  }
  if(!m_setting || step < 0 || step >= std::ssize(m.steps()))
    return std::nullopt;
  auto vec = m.steps();
  vec[step] = ossia::clamp(float(pos.y() / height), 0.f, 1.f);
  if(vec == m.steps())
    return std::nullopt;
  return vec;
}

void StepsGesture::release() noexcept
{
  m_drag.stop();
  m_setting = false;
}

void randomizeSteps(const Model& m, const score::CommandStackFacade& stack)
{
  std::mt19937 rng{std::random_device{}()};
  std::uniform_real_distribution<float> dist{0.f, 1.f};
  auto vec = m.steps();
  for(auto& v : vec)
    v = dist(rng);
  CommandDispatcher<>{stack}.submit<ChangeSteps>(m, vec);
}

void fillStepsMenu(QMenu& menu, const Model& m, const score::CommandStackFacade& stack)
{
  auto act = menu.addAction(QObject::tr("Randomize"));
  QObject::connect(act, &QAction::triggered, &menu, [&m, &stack] {
    randomizeSteps(m, stack);
  });
}

}
