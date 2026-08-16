#include <score/graphics/InfiniteScroller.hpp>
#include <score/graphics/widgets/QGraphicsCombo.hpp>
#include <score/model/Skin.hpp>
#include <score/tools/Cursor.hpp>
#include <score/widgets/ComboBox.hpp>

#include <ossia/detail/math.hpp>

#include <QApplication>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPointer>
#include <QScreen>
#include <QTimer>

#include <memory>
#include <utility>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsCombo);

namespace score
{
struct DefaultComboImpl
{
  static bool draggable(const QGraphicsCombo& self) noexcept
  {
    return self.array.size() > 1;
  }

  static int positionToIndex(const QGraphicsCombo& self, double v) noexcept
  {
    const int last = int(self.array.size()) - 1;
    return std::clamp(int(std::round(v * last)), 0, last);
  }

  static void mousePressEvent(QGraphicsCombo& self, QGraphicsSceneMouseEvent* event)
  {
    if(event->button() == Qt::LeftButton && draggable(self))
    {
      self.m_grab = true;
      InfiniteScroller::start(self, double(self.m_value) / (self.array.size() - 1));
    }

    event->accept();
  }

  static void mouseMoveEvent(QGraphicsCombo& self, QGraphicsSceneMouseEvent* event)
  {
    if((event->buttons() & Qt::LeftButton) && self.m_grab)
    {
      int curPos = positionToIndex(self, InfiniteScroller::move(event));
      if(curPos != self.m_value)
      {
        self.m_value = curPos;
        self.sliderMoved();
        self.update();
      }
    }
    event->accept();
  }

  static void mouseReleaseEvent(QGraphicsCombo& self, QGraphicsSceneMouseEvent* event)
  {
    if(event->button() == Qt::LeftButton)
    {
      if(self.m_grab)
      {
        int curPos = positionToIndex(self, InfiniteScroller::move(event));
        if(curPos != self.m_value)
        {
          self.m_value = curPos;
          self.update();
        }
        self.m_grab = false;
      }
      InfiniteScroller::stop(self, event);
      self.sliderReleased();
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
    maxW = std::max(r.width() + 8., maxW);
  }
  m_rect.setWidth(maxW);
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

    QObject::connect(w, &QComboBox::activated, w, [done, commit, close](int idx) {
      if(*done)
        return;
      commit(idx);
      close();
    });

    QObject::connect(w, &ComboBoxWithEnter::editingCancelled, w, [done, close] {
      if(*done)
        return;
      close();
    });

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

  // Draw text
  painter->setPen(skin.Base4.main.pen2);
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->setFont(skin.Medium10Pt);
  if(int n = value(); n >= 0 && n < array.size())
  {
    painter->drawText(brect, array[value()], QTextOption(Qt::AlignCenter));
  }

  painter->drawLine(2, 2, 2, boundingRect().height() - 2);
}
}
