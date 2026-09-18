#include <score/graphics/InfiniteScroller.hpp>
#include <score/graphics/widgets/QGraphicsCombo.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Cursor.hpp>
#include <score/widgets/ComboBox.hpp>

#include <ossia/detail/math.hpp>

#include <QApplication>
#include <QAbstractItemView>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QTimer>

#include <cmath>

#include <memory>
#include <utility>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsCombo);

namespace score
{
namespace
{
//! Reports the drop-down list being hidden. QComboBox has no signal for it, and
//! dismissing the list by clicking away is otherwise indistinguishable from
//! leaving it open: no activation, and no focus change the box hears about.
struct PopupDismissWatcher final : QObject
{
  using QObject::QObject;
  std::function<void()> onHide;

  bool eventFilter(QObject* watched, QEvent* event) override
  {
    if(event->type() == QEvent::Hide && onHide)
      onHide();
    return QObject::eventFilter(watched, event);
  }
};
}

struct DefaultComboImpl
{
  static bool draggable(const QGraphicsCombo& self) noexcept
  {
    return self.array.size() > 1;
  }

  static int positionToIndex(const QGraphicsCombo& self, double v) noexcept
  {
    const int last = int(self.array.size()) - 1;
    if(last < 0)
      return 0;
    return std::clamp(int(std::round(v * last)), 0, last);
  }

  //! Has the pointer travelled far enough since the press to mean "scrub"
  //! rather than "click"? Below the platform's drag threshold a wobble of a
  //! pixel or two is a click, and must not nudge the value.
  static bool passedDragThreshold(QGraphicsSceneMouseEvent* event) noexcept
  {
    const auto delta
        = event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton);
    return delta.manhattanLength() >= QApplication::startDragDistance();
  }

  //! +1 for the upper half of the stepper strip, -1 for the lower half, 0 if
  //! the point is not on the strip at all.
  static int stepAt(const QGraphicsCombo& self, QPointF pos) noexcept
  {
    // Not drawn either: the box stays a plain click-to-open-the-list.
    if(!draggable(self))
      return 0;

    if(!self.stepperVisible())
      return 0;
    const QRectF r = self.stepperRect();
    if(!r.contains(pos))
      return 0;
    return pos.y() < r.center().y() ? +1 : -1;
  }

  static void mousePressEvent(QGraphicsCombo& self, QGraphicsSceneMouseEvent* event)
  {
    if(event->button() == Qt::LeftButton)
    {
      self.m_dragged = false;

      // The stepper swallows the press: no scrub, no drop-down on release.
      if(const int step = stepAt(self, event->pos()); step != 0)
      {
        self.m_pressedStep = step;
        self.m_stepArmed = true;
        self.update();
        event->accept();
        return;
      }

      if(draggable(self))
      {
        self.m_grab = true;
        InfiniteScroller::start(self, double(self.m_value) / (self.array.size() - 1));
      }
    }

    event->accept();
  }

  static void mouseMoveEvent(QGraphicsCombo& self, QGraphicsSceneMouseEvent* event)
  {
    if(self.m_pressedStep != 0)
    {
      // Leaving un-presses it, as everywhere else; the release is then a no-op.
      const bool armed = stepAt(self, event->pos()) == self.m_pressedStep;
      if(armed != self.m_stepArmed)
      {
        self.m_stepArmed = armed;
        self.update();
      }
      event->accept();
      return;
    }

    if(event->buttons() & Qt::LeftButton)
    {
      if(!self.m_dragged && passedDragThreshold(event))
        self.m_dragged = true;

      if(self.m_grab && self.m_dragged)
      {
        int curPos = positionToIndex(self, InfiniteScroller::move(event));
        if(curPos != self.m_value)
        {
          self.m_value = curPos;
          self.sliderMoved();
          self.update();
        }
      }
    }
    event->accept();
  }

  static void mouseReleaseEvent(QGraphicsCombo& self, QGraphicsSceneMouseEvent* event)
  {
    if(event->button() == Qt::LeftButton)
    {
      if(const int step = std::exchange(self.m_pressedStep, 0); step != 0)
      {
        self.update();
        if(std::exchange(self.m_stepArmed, false))
          self.step(step);
        event->accept();
        return;
      }

      const bool wasDrag = self.m_dragged;
      if(self.m_grab)
      {
        if(wasDrag)
        {
          int curPos = positionToIndex(self, InfiniteScroller::move(event));
          if(curPos != self.m_value)
          {
            self.m_value = curPos;
            self.update();
          }
        }
        self.m_grab = false;
      }
      self.m_dragged = false;
      InfiniteScroller::stop(self, event);
      self.sliderReleased();

      // A click that did not scrub opens the drop-down, the way a combo box
      // does everywhere else. Nothing to pick from means nothing to open.
      if(!wasDrag && !self.array.isEmpty())
        self.openEditor(event->scenePos());
    }
    else if(event->button() == Qt::RightButton)
    {
      self.openEditor(event->scenePos());
    }
    event->accept();
  }

  //! QEvent::UngrabMouse: the scene took the implicit grab away and there will
  //! be no release to end the drag on. See DefaultGraphicsSliderImpl for what
  //! goes wrong if the edit is left open.
  static void ungrabMouseEvent(QGraphicsCombo& self, QEvent* event)
  {
    // The release that would have cleared these is never coming.
    self.m_dragged = false;
    self.m_stepArmed = false;
    if(std::exchange(self.m_pressedStep, 0) != 0)
      self.update();

    if(!self.m_grab)
      return;

    self.m_grab = false;
    InfiniteScroller::abort(self);
    self.sliderReleased();
  }
};

QGraphicsCombo::QGraphicsCombo(QGraphicsItem* parent)
    : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorSpin);
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
  this->setAcceptHoverEvents(true);
}

void QGraphicsCombo::init()
{
  prepareGeometryChange();

  auto& skin = score::Skin::instance();
  QFontMetricsF metrics{skin.Medium10Pt};
  double maxW = m_rect.width();

  for(auto& value : this->array)
  {
    auto r = metrics.boundingRect(value);
    maxW = std::max(r.width() + 8. + stepperWidth, maxW);
  }
  m_rect.setWidth(maxW);
}

QRectF QGraphicsCombo::stepperRect() const noexcept
{
  const QRectF brect = m_rect.adjusted(1, 1, -1, -1);
  const double left = std::max(brect.left(), brect.right() - stepperWidth);
  return QRectF{left, brect.top(), brect.right() - left, brect.height()};
}

bool QGraphicsCombo::stepperVisible() const noexcept
{
  // Below this the strip takes the whole box and leaves the text nowhere.
  return array.size() > 1 && m_rect.width() > 3. * stepperWidth;
}

void QGraphicsCombo::step(int n)
{
  const int sz = int(array.size());
  if(sz <= 1 || n == 0)
    return;

  // Wraps: the point is to walk the list without opening the drop-down.
  const int next = ((m_value + n) % sz + sz) % sz;
  if(next == m_value)
    return;

  m_value = next;
  update();
  sliderMoved();
  sliderReleased();
}

void QGraphicsCombo::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsCombo::setValue(int v)
{
  if(array.empty())
    return;
  m_value = ossia::clamp(v, 0, int(array.size() - 1));
  update();
}

int QGraphicsCombo::value() const
{
  return m_value;
}

void QGraphicsCombo::setEditable(bool b)
{
  m_editable = b;
}

void QGraphicsCombo::openEditor(QPointF scenePos)
{
  auto build = [self = QPointer{this}, scenePos] {
    if(!self || !self->scene())
      return;

    // One drop-down at a time. Both buttons open one and the build is deferred,
    // so a second click arriving first would otherwise stack another editor on
    // top of this one, and closing the pair double-frees the proxy.
    if(self->m_editor)
      return;

    auto& item = *self;
    auto w = new ComboBoxWithEnter;
    w->addItems(item.array);
    w->setEditable(item.m_editable);
    if(item.m_editable)
      w->setInsertPolicy(QComboBox::NoInsert);
    w->setCurrentIndex(item.m_value);

    auto* scene = item.scene();
    auto obj = scene->addWidget(w, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
    obj->setPos(scenePos);
    item.m_editor = obj;

#if defined(__EMSCRIPTEN__)
    w->setFocus();
    w->showPopup();
#else
    QTimer::singleShot(0, w, [w] {
      w->setFocus();
      w->showPopup();
    });
#endif

    auto done = std::make_shared<bool>(false);
    auto close = [done, proxy = QPointer{obj}] {
      if(std::exchange(*done, true) || !proxy)
        return;
      QTimer::singleShot(0, proxy.data(), [proxy] {
        if(!proxy)
          return;
        if(auto* sc = proxy->scene())
          sc->removeItem(proxy);
        delete proxy.data();
      });
    };

    auto commit = [self](int idx) {
      if(!self || idx < 0 || idx >= self->array.size())
        return;
      self->m_value = idx;
      self->update();
      self->sliderMoved();
      self->sliderReleased();
    };

    // The editor lists a snapshot of the items. A runtime-populated combo box
    // can be repopulated while the drop-down is open, so resolve what the user
    // picked by its text against the list as it stands now: the same index may
    // mean a different entry, or none at all.
    auto commitByText = [self, commit](const QString& text) {
      if(!self)
        return;
      const int idx = self->array.indexOf(text);
      if(idx >= 0)
        commit(idx);
    };

    QObject::connect(
        w, &QComboBox::activated, w, [done, commitByText, close, w](int idx) {
      if(*done)
        return;
      if(idx >= 0 && idx < w->count())
        commitByText(w->itemText(idx));
      close();
    });

    QObject::connect(w, &ComboBoxWithEnter::editingCancelled, w, [done, close] {
      if(*done)
        return;
      close();
    });

    // Dismissing the list by clicking away leaves a non-editable box with
    // nothing to do: it exists only to pick from that list, and ComboBoxWithEnter
    // only reports a focus change while the list is down, so nothing else would
    // ever take it off the scene. Deferred by one turn because the list hides
    // before `activated` arrives, and that must still be allowed to commit.
    if(!item.m_editable)
    {
      auto* watcher = new PopupDismissWatcher{w};
      watcher->onHide = [done, close, w = QPointer{w}] {
        if(*done)
          return;
        QTimer::singleShot(0, w, [done, close] {
          if(!*done)
            close();
        });
      };
      w->view()->installEventFilter(watcher);
    }

    QObject::connect(
        w, &ComboBoxWithEnter::editingFinished, w, [self, done, commit, close, w] {
      if(*done)
        return;

      if(self && self->m_editable)
      {
        const QString text = w->currentText();
        if(const int idx = self->array.indexOf(text); idx >= 0)
          commit(idx);
        else if(!text.isEmpty())
          self->valueEdited(text);
      }
      close();
        });
  };

#if defined(__EMSCRIPTEN__)
  build();
#else
  QTimer::singleShot(0, this, build);
#endif
}

void QGraphicsCombo::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void QGraphicsCombo::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
  auto& skin = score::Skin::instance();
  const bool onStepper = stepperVisible() && stepperRect().contains(event->pos());
  setCursor(onStepper ? skin.CursorPointingHand : skin.CursorSpin);
  event->accept();
}

void QGraphicsCombo::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultComboImpl::mousePressEvent(*this, event);
  event->accept();
}

void QGraphicsCombo::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultComboImpl::mouseMoveEvent(*this, event);
  event->accept();
}

void QGraphicsCombo::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultComboImpl::mouseReleaseEvent(*this, event);
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorSpin);
  event->accept();
}

bool QGraphicsCombo::sceneEvent(QEvent* event)
{
  if(event->type() == QEvent::UngrabMouse)
  {
    DefaultComboImpl::ungrabMouseEvent(*this, event);
    auto& skin = score::Skin::instance();
    setCursor(skin.CursorSpin);
  }
  return QGraphicsItem::sceneEvent(event);
}

QRectF QGraphicsCombo::boundingRect() const
{
  return m_rect;
}

void QGraphicsCombo::paint(
    QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  painter->setPen(skin.NoPen);
  painter->setBrush(skin.Emphasis2.main.brush);

  // Draw rect
  const QRectF brect = boundingRect().adjusted(1, 1, -1, -1);
  painter->drawRoundedRect(brect, 1, 1);

  const bool hasStepper = stepperVisible();
  const QRectF textRect
      = hasStepper ? brect.adjusted(0, 0, -stepperWidth, 0) : brect;

  // Draw text
  painter->setPen(skin.Base4.main.pen2);
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->setFont(skin.Medium10Pt);
  if(int n = value(); n >= 0 && n < array.size())
  {
    painter->drawText(textRect, array[value()], QTextOption(Qt::AlignCenter));
  }

  painter->drawLine(2, 2, 2, boundingRect().height() - 2);

  if(hasStepper)
    paintStepper(*painter, skin);
}

void QGraphicsCombo::paintStepper(QPainter& painter, const score::Skin& skin)
{
  const QRectF strip = stepperRect();
  const QRectF halves[2]
      = {QRectF{strip.topLeft(), QSizeF{strip.width(), strip.height() / 2.}},
         QRectF{
             QPointF{strip.left(), strip.top() + strip.height() / 2.},
             QSizeF{strip.width(), strip.height() / 2.}}};

  // Half the glyph's arm length, so that + and - are the same width.
  const double arm = 2.;
  for(int i = 0; i < 2; i++)
  {
    const int step = i == 0 ? +1 : -1;
    const QRectF& half = halves[i];

    if(m_pressedStep == step && m_stepArmed)
    {
      painter.setPen(skin.NoPen);
      painter.setBrush(skin.Emphasis1.main.brush);
      painter.drawRect(half);
    }

    // Rounded: a one-pixel pen with antialiasing off needs a whole pixel, and
    // the two glyphs have to line up with each other.
    const QPointF c{std::round(half.center().x()), std::round(half.center().y())};
    painter.setPen(skin.Base4.main.pen1);
    painter.drawLine(QPointF{c.x() - arm, c.y()}, QPointF{c.x() + arm, c.y()});
    if(step > 0)
      painter.drawLine(QPointF{c.x(), c.y() - arm}, QPointF{c.x(), c.y() + arm});
  }
}
}
