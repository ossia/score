#include "GraphicWidgets.hpp"

#include <score/actions/ActionManager.hpp>
#include <score/graphics/DefaultGraphicsKnobImpl.hpp>
#include <score/graphics/DefaultGraphicsSliderImpl.hpp>
#include <score/graphics/GraphicsSliderBaseImpl.hpp>
#include <score/model/Skin.hpp>

#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QListView>
#include <QWidget>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsPixmapButton)
W_OBJECT_IMPL(score::QGraphicsSelectablePixmapToggle)
W_OBJECT_IMPL(score::QGraphicsPixmapToggle)
W_OBJECT_IMPL(score::QGraphicsSlider)
W_OBJECT_IMPL(score::QGraphicsKnob)
W_OBJECT_IMPL(score::QGraphicsLogSlider)
W_OBJECT_IMPL(score::QGraphicsLogKnob)
W_OBJECT_IMPL(score::QGraphicsIntSlider)
W_OBJECT_IMPL(score::QGraphicsCombo)
W_OBJECT_IMPL(score::QGraphicsEnum)
W_OBJECT_IMPL(score::QGraphicsHSVChooser)
W_OBJECT_IMPL(score::QGraphicsXYChooser)
W_OBJECT_IMPL(score::QGraphicsCheckBox)

namespace score
{

QGraphicsPixmapButton::QGraphicsPixmapButton(
    QPixmap pressed,
    QPixmap released,
    QGraphicsItem* parent)
    : QGraphicsPixmapItem{released, parent}
    , m_pressed{std::move(pressed)}
    , m_released{std::move(released)}
{
  // TODO https://bugreports.qt.io/browse/QTBUG-77970
  setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
}

void QGraphicsPixmapButton::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  setPixmap(m_pressed);
  event->accept();
}

void QGraphicsPixmapButton::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsPixmapButton::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  setPixmap(m_released);
  clicked();
  event->accept();
}

QGraphicsSelectablePixmapToggle::QGraphicsSelectablePixmapToggle(
    QPixmap pressed,
    QPixmap pressed_selected,
    QPixmap released,
    QPixmap released_selected,
    QGraphicsItem* parent)
    : QGraphicsPixmapItem{released, parent}
    , m_pressed{std::move(pressed)}
    , m_pressed_selected{std::move(pressed_selected)}
    , m_released{std::move(released)}
    , m_released_selected{std::move(released_selected)}
{
  // TODO https://bugreports.qt.io/browse/QTBUG-77970
  setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
  setCursor(Qt::CrossCursor);
}

void QGraphicsSelectablePixmapToggle::toggle()
{
  m_toggled = !m_toggled;
  setPixmap(
      m_toggled ? (m_selected ? m_pressed_selected : m_pressed)
                : (m_selected ? m_released_selected : m_released));
}

void QGraphicsSelectablePixmapToggle::setSelected(bool selected)
{
  if (selected != m_selected)
  {
    m_selected = selected;
    setPixmap(
        m_toggled ? (m_selected ? m_pressed_selected : m_pressed)
                  : (m_selected ? m_released_selected : m_released));
  }
}

void QGraphicsSelectablePixmapToggle::setState(bool toggled)
{
  if (toggled != m_toggled)
  {
    m_toggled = toggled;
    setPixmap(
        m_toggled ? (m_selected ? m_pressed_selected : m_pressed)
                  : (m_selected ? m_released_selected : m_released));
  }
}

void QGraphicsSelectablePixmapToggle::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_toggled = !m_toggled;
  setPixmap(
      m_toggled ? (m_selected ? m_pressed_selected : m_pressed)
                : (m_selected ? m_released_selected : m_released));
  toggled(m_toggled);
  event->accept();
}

void QGraphicsSelectablePixmapToggle::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsSelectablePixmapToggle::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

QGraphicsPixmapToggle::QGraphicsPixmapToggle(
    QPixmap pressed,
    QPixmap released,
    QGraphicsItem* parent)
    : QGraphicsPixmapItem{released, parent}
    , m_pressed{std::move(pressed)}
    , m_released{std::move(released)}
{
  // TODO https://bugreports.qt.io/browse/QTBUG-77970
  setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
  setCursor(Qt::CrossCursor);
}

void QGraphicsPixmapToggle::toggle()
{
  m_toggled = !m_toggled;
  setPixmap(m_toggled ? m_pressed : m_released);
}

void QGraphicsPixmapToggle::setState(bool toggled)
{
  if (toggled != m_toggled)
  {
    m_toggled = toggled;
    setPixmap(m_toggled ? m_pressed : m_released);
  }
}

void QGraphicsPixmapToggle::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_toggled = !m_toggled;
  setPixmap(m_toggled ? m_pressed : m_released);
  toggled(m_toggled);
  event->accept();
}

void QGraphicsPixmapToggle::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsPixmapToggle::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

template void QGraphicsSliderBase<QGraphicsSlider>::setRect(const QRectF& r);
template void QGraphicsSliderBase<QGraphicsLogSlider>::setRect(const QRectF& r);
template void QGraphicsSliderBase<QGraphicsIntSlider>::setRect(const QRectF& r);

QGraphicsSlider::QGraphicsSlider(QGraphicsItem* parent) : QGraphicsSliderBase{parent} { }

void QGraphicsSlider::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double QGraphicsSlider::value() const
{
  return m_value;
}

void QGraphicsSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mousePressEvent(*this, event);
}

void QGraphicsSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseMoveEvent(*this, event);
}

void QGraphicsSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsSlider::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void QGraphicsSlider::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseDoubleClickEvent(*this, event);
}

void QGraphicsSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsSliderImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(min + value() * (max - min), 'f', 3),
      painter,
      widget);
}

QGraphicsKnob::QGraphicsKnob(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsKnob::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsKnob::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double QGraphicsKnob::value() const
{
  return m_value;
}

void QGraphicsKnob::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mousePressEvent(*this, event);
}

void QGraphicsKnob::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseMoveEvent(*this, event);
}

void QGraphicsKnob::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsKnob::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void QGraphicsKnob::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseDoubleClickEvent(*this, event);
}

QRectF QGraphicsKnob::boundingRect() const
{
  return m_rect;
}

void QGraphicsKnob::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsKnobImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(min + value() * (max - min), 'f', 2),
      painter,
      widget);
}

QGraphicsLogSlider::QGraphicsLogSlider(QGraphicsItem* parent) : QGraphicsSliderBase{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsLogSlider::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double QGraphicsLogSlider::value() const
{
  return m_value;
}

double QGraphicsLogSlider::map(double v) const noexcept
{
  return ossia::normalized_to_log(min, max - min, v);
}

double QGraphicsLogSlider::unmap(double v) const noexcept
{
  return ossia::log_to_normalized(min, max - min, v);
}

void QGraphicsLogSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mousePressEvent(*this, event);
}

void QGraphicsLogSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseMoveEvent(*this, event);
}

void QGraphicsLogSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsLogSlider::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void QGraphicsLogSlider::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsSliderImpl::mouseDoubleClickEvent(*this, event);
}

void QGraphicsLogSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsSliderImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(ossia::normalized_to_log(min, max - min, value()), 'f', 3),
      painter,
      widget);
}

QGraphicsLogKnob::QGraphicsLogKnob(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsLogKnob::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsLogKnob::setValue(double v)
{
  m_value = ossia::clamp(v, 0., 1.);
  update();
}

double QGraphicsLogKnob::value() const
{
  return m_value;
}

double QGraphicsLogKnob::map(double v) const noexcept
{
  return ossia::normalized_to_log(min, max - min, v);
}

double QGraphicsLogKnob::unmap(double v) const noexcept
{
  return ossia::log_to_normalized(min, max - min, v);
}

void QGraphicsLogKnob::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mousePressEvent(*this, event);
}

void QGraphicsLogKnob::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseMoveEvent(*this, event);
}

void QGraphicsLogKnob::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseReleaseEvent(*this, event);
}

void QGraphicsLogKnob::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
{
  event->accept();
}

void QGraphicsLogKnob::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
{
  DefaultGraphicsKnobImpl::mouseDoubleClickEvent(*this, event);
}

QRectF QGraphicsLogKnob::boundingRect() const
{
  return m_rect;
}

void QGraphicsLogKnob::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsKnobImpl::paint(
      *this,
      score::Skin::instance(),
      QString::number(ossia::normalized_to_log(min, max - min, value()), 'f', 3),
      painter,
      widget);
}

QGraphicsIntSlider::QGraphicsIntSlider(QGraphicsItem* parent) : QGraphicsSliderBase{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);
}

void QGraphicsIntSlider::setValue(int v)
{
  m_value = ossia::clamp(v, m_min, m_max);
  update();
}

void QGraphicsIntSlider::setRange(int min, int max)
{
  m_min = min;
  m_max = max;
  update();
}

int QGraphicsIntSlider::value() const
{
  return m_value;
}

void QGraphicsIntSlider::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  if (isInHandle(event->pos()))
  {
    m_grab = true;
  }

  const auto srect = sliderRect();
  double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
  int res = std::floor(m_min + curPos * (m_max - m_min));
  if (res != m_value)
  {
    m_value = res;
    valueChanged(m_value);
    sliderMoved();
    update();
  }

  event->accept();
}

void QGraphicsIntSlider::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    const auto srect = sliderRect();
    double curPos = ossia::clamp(event->pos().x(), 0., srect.width()) / srect.width();
    int res = std::floor(m_min + curPos * (m_max - m_min));
    if (res != m_value)
    {
      m_value = res;
      valueChanged(m_value);
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsIntSlider::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    double curPos = ossia::clamp(event->pos().x() / sliderRect().width(), 0., 1.);
    int res = std::floor(m_min + curPos * (m_max - m_min));
    if (res != m_value)
    {
      m_value = res;
      valueChanged(m_value);
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  event->accept();
}

void QGraphicsIntSlider::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  DefaultGraphicsSliderImpl::paint(
      *this, score::Skin::instance(), QString::number(value()), painter, widget);
}

double QGraphicsIntSlider::getHandleX() const
{
  return sliderRect().width() * ((double(m_value) - m_min) / (m_max - m_min));
}

struct SCORE_LIB_BASE_EXPORT ComboBoxWithEnter final : public QComboBox
{
  W_OBJECT(ComboBoxWithEnter)

public:
  ComboBoxWithEnter(QWidget* parent = nullptr) : QComboBox{parent} { }

  void editingFinished() W_SIGNAL(editingFinished)

private:
  bool eventFilter(QObject* watched, QEvent* event) override
  {
    if (event->type() == QEvent::FocusOut)
    {
      editingFinished();
    }

    return false;
  }

  bool event(QEvent* event) override
  {
    auto res = QComboBox::event(event);
    if (event->type() == QEvent::ShortcutOverride)
    {
      QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
      switch (keyEvent->key())
      {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
          editingFinished();
        default:
          break;
      }
    }
    return res;
  }

  void focusInEvent(QFocusEvent* event) override { QComboBox::focusInEvent(event); }

  void focusOutEvent(QFocusEvent* event) override
  {
    QComboBox::focusOutEvent(event);
    auto lv = findChildren<QListView*>().front();
    if (lv)
    {
      lv->installEventFilter(this);
      QTimer::singleShot(100, this, [this] {
        auto lv = findChildren<QListView*>().front();
        if (!lv->hasFocus())
        {
          editingFinished();
        }
      });
    }
    else
    {
      editingFinished();
    }
  }
};

struct DefaultComboImpl
{
  static inline double origValue{};
  static inline double currentDelta{};
  static inline QRectF currentGeometry{};

  static void mousePressEvent(QGraphicsCombo& self, QGraphicsSceneMouseEvent* event)
  {
    currentDelta = 0.;
    if (event->button() == Qt::LeftButton)
    {
      self.m_grab = true;
      self.setCursor(QCursor(Qt::BlankCursor));
      origValue = double(self.m_value) / (self.array.size() - 1);
      currentGeometry = qApp->primaryScreen()->availableGeometry();
    }

    event->accept();
  }

  static void mouseMoveEvent(QGraphicsCombo& self, QGraphicsSceneMouseEvent* event)
  {
    if ((event->buttons() & Qt::LeftButton) && self.m_grab)
    {
      auto delta = (event->screenPos().y() - event->lastScreenPos().y());
      double ratio = qApp->keyboardModifiers() & Qt::CTRL ? 0.2 : 1.;
      if (std::abs(delta) < 500)
      {
        currentDelta += ratio * delta;
      }

      const double max = currentGeometry.height();

      if (event->screenPos().y() <= 0)
        score::setCursorPos(QPointF(event->screenPos().x(), max));
      else if (event->screenPos().y() >= max)
        score::setCursorPos(QPointF(event->screenPos().x(), 0));

      double v = origValue - currentDelta / max;
      if (v <= 0.)
      {
        currentDelta = origValue * max;
        v = 0.;
      }
      else if (v >= 1.)
      {
        currentDelta = ((origValue - 1.) * max);
        v = 1.;
      }

      int curPos = v * (self.array.size() - 1);
      if (curPos != self.m_value)
      {
        self.m_value = curPos;
        self.valueChanged(self.m_value);
        self.sliderMoved();
        self.update();
      }
    }
    event->accept();
  }

  static void mouseReleaseEvent(QGraphicsCombo& self, QGraphicsSceneMouseEvent* event)
  {
    if (event->button() == Qt::LeftButton)
    {
      score::setCursorPos(event->buttonDownScreenPos(Qt::LeftButton));
      self.unsetCursor();

      if (self.m_grab)
      {
        auto delta = (event->screenPos().y() - event->lastScreenPos().y());
        double ratio = qApp->keyboardModifiers() & Qt::CTRL ? 0.2 : 1.;
        if (std::abs(delta) < 500)
          currentDelta += ratio * delta;

        double v = origValue - currentDelta / currentGeometry.height();
        int curPos = ossia::clamp(v, 0., 1.) * (self.array.size() - 1);
        if (curPos != self.m_value)
        {
          self.m_value = curPos;
          self.valueChanged(self.m_value);
          self.update();
        }
        self.m_grab = false;
      }
      self.sliderReleased();
    }
    else if (event->button() == Qt::RightButton)
    {
      QTimer::singleShot(0, [&, pos = event->scenePos()] {
        auto w = new ComboBoxWithEnter;
        w->addItems(self.array);
        w->setCurrentIndex(self.m_value);

        auto obj = self.scene()->addWidget(w, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
        obj->setPos(pos);

        QTimer::singleShot(0, w, [w] { w->setFocus(); });

        QObject::connect(
            w, SignalUtils::QComboBox_currentIndexChanged_int(), &self, [=, &self](int v) {
              self.m_value = v;
              self.valueChanged(self.m_value);
              self.sliderMoved();
              self.update();
            });

        QObject::connect(w, &ComboBoxWithEnter::editingFinished, &self, [obj, &self]() mutable {
          if (obj != nullptr)
          {
            self.sliderReleased();
            QTimer::singleShot(0, obj, [scene = self.scene(), obj] {
              scene->removeItem(obj);
              delete obj;
            });
          }
          obj = nullptr;
        });
      });
    }
    event->accept();
  }
};

QGraphicsCombo::QGraphicsCombo(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);
}

void QGraphicsCombo::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
}

void QGraphicsCombo::setValue(int v)
{
  m_value = ossia::clamp(v, 0, array.size() - 1);
  update();
}

int QGraphicsCombo::value() const
{
  return m_value;
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
  event->accept();
}

QRectF QGraphicsCombo::boundingRect() const
{
  return m_rect;
}

void QGraphicsCombo::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  painter->setPen(skin.NoPen);
  painter->setBrush(skin.Background1.main.brush);

  // Draw rect
  const QRectF brect = boundingRect().adjusted(1, 1, -1, -1);
  painter->drawRoundedRect(brect, 1, 1);

  // Draw text
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->setPen(skin.Base4.lighter180.pen1);
  painter->setFont(skin.Medium8Pt);
  painter->drawText(brect, array[value()], QTextOption(Qt::AlignCenter));
}

QGraphicsEnum::QGraphicsEnum(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  this->setAcceptedMouseButtons(Qt::LeftButton);
  setRect({0, 0, 150, 45});
}

void QGraphicsEnum::setRect(const QRectF& r)
{
  prepareGeometryChange();
  m_rect = r;
  m_smallRect = r.adjusted(2, 2, -2, -2);
}

void QGraphicsEnum::setValue(int v)
{
  m_value = ossia::clamp(v, 0, (int)array.size() - 1);
  update();
}

int QGraphicsEnum::value() const
{
  return m_value;
}

void QGraphicsEnum::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  int row = 0;
  int col = 0;
  int actual_rows = std::ceil(double(array.size()) / columns);
  const double w = m_smallRect.width() / columns;
  const double h = m_smallRect.height() / actual_rows;

  for (int i = 0; i < array.size(); i++)
  {
    QRectF rect{2. + col * w, 2. + row * h, w - 1., h - 1.};
    if (rect.contains(event->pos()))
    {
      m_clicking = i;
      update();
      return;
    }

    col++;
    if (col == columns)
    {
      row++;
      col = 0;
    }
  }

  m_clicking = -1;
  update();
}

void QGraphicsEnum::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsEnum::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
  if (m_clicking != -1)
  {
    m_value = m_clicking;
    m_clicking = -1;
    currentIndexChanged(m_value);
  }
  update();
}

QRectF QGraphicsEnum::boundingRect() const
{
  return m_rect;
}

void QGraphicsEnum::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  auto& style = score::Skin::instance();

  const QPen& text = style.Gray.main.pen1;
  const QFont& textFont = style.MonoFontSmall;
  const QPen& currentText = style.Base4.lighter180.pen1;
  const QBrush& bg = style.SliderBrush;
  const QPen& noPen = style.NoPen;

  int actual_rows = std::ceil(double(array.size()) / columns);
  int row = 0;
  int col = 0;
  const double w = m_smallRect.width() / columns;
  const double h = m_smallRect.height() / actual_rows;

  painter->setBrush(bg);
  int i = 0;
  QRectF clickRect{};
  for (const QString& str : array)
  {
    QRectF rect{2. + col * w, 2. + row * h, w - 1., h - 1.};
    if (i == m_clicking)
    {
      clickRect = rect;
      painter->setPen(currentText);
    }
    else
    {
      painter->setPen(noPen);
      painter->drawRect(rect);
      painter->setPen(i != m_value ? text : currentText);
    }

    painter->setFont(textFont);
    painter->drawText(rect, str, QTextOption(Qt::AlignCenter));
    col++;
    if (col == columns)
    {
      row++;
      col = 0;
    }
    i++;
  }
}

QGraphicsPixmapEnum::QGraphicsPixmapEnum(QGraphicsItem* parent) : QGraphicsEnum{parent} { }

void QGraphicsPixmapEnum::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  painter->setRenderHint(QPainter::Antialiasing, false);
  auto& style = score::Skin::instance();

  const QBrush& bg = style.SliderBrush;
  const QPen& noPen = style.NoPen;

  int actual_rows = std::ceil(double(array.size()) / columns);
  int row = 0;
  int col = 0;
  const double w = m_smallRect.width() / columns;
  const double h = m_smallRect.height() / actual_rows;

  painter->setBrush(bg);
  int i = 0;
  for (std::size_t img = 0; img < off_images.size(); img++)
  {
    QRectF rect{4. + col * w, 4. + row * h, w - 3., h - 3.};
    if (i != m_clicking)
    {
      painter->setPen(noPen);
      painter->drawRect(rect);
    }

    const QPixmap& pixmap = i != m_value ? on_images[img] : off_images[img];
    painter->drawPixmap(rect.topLeft(), pixmap);
    col++;
    if (col == columns)
    {
      row++;
      col = 0;
    }
    i++;
  }
}

QGraphicsHSVChooser::QGraphicsHSVChooser(QGraphicsItem* parent) { }

void QGraphicsHSVChooser::setRect(const QRectF& r)
{
  SCORE_TODO;
}

namespace
{
static QRgb hsvColors[100][100];
static QRgb valueColors[100];
static auto initHsvColors = [] {
  for (int j = 0; j < 100; j++)
  {
    for (int i = 0; i < 100; i++)
    {
      const QRgb col = QColor::fromHsvF(double(i) / 100., double(j) / 100., 1.).rgba();
      hsvColors[i][j] = col;
    }
  }

  for (int j = 0; j < 100; j++)
  {
    const QRgb col = QColor::fromHsvF(-1., 1., double(j) / 100.).rgba();
    valueColors[j] = col;
  }
  return 0;
}();
}
void QGraphicsHSVChooser::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  static QImage hs_zone{100, 100, QImage::Format_ARGB32};
  {
    auto img_data = hs_zone.bits();
    for (int j = 0; j < 100; j++)
    {
      for (int i = 0; i < 100; i++)
      {
        const QRgb col = hsvColors[i][j];
        img_data[0] = qBlue(col) * this->v;
        img_data[1] = qGreen(col) * this->v;
        img_data[2] = qRed(col) * this->v;
        img_data[3] = 255;
        img_data += 4;
      }
    }
  }

  static QImage v_zone{20, 100, QImage::Format_ARGB32};
  {
    auto img_data = v_zone.bits();
    for (int j = 0; j < 100; j++)
    {
      const QRgb col = valueColors[j];
      for (int i = 0; i < 20; i++)
      {
        img_data[0] = qBlue(col);
        img_data[1] = qGreen(col);
        img_data[2] = qRed(col);
        img_data[3] = 255;
        img_data += 4;
      }
    }
  }

  painter->drawImage(QPointF{0, 0}, hs_zone);
  painter->drawImage(QPointF{110, 0}, v_zone);

  const auto color = QColor::fromRgbF(m_value[0], m_value[1], m_value[2]).toHsv();
  auto x = color.hsvHueF() * 100.;
  auto y = color.hsvSaturationF() * 100.;
  auto val_y = color.valueF() * 100.;

  painter->setPen(score::Skin::instance().DarkGray.main.pen0);
  painter->drawLine(QPointF{x, 0.}, QPointF{x, 100.});
  painter->drawLine(QPointF{0, y}, QPointF{100., y});

  painter->drawLine(QPointF{111., val_y}, QPointF{130., val_y});
}

std::array<float, 4> QGraphicsHSVChooser::value() const
{
  return m_value;
}

void QGraphicsHSVChooser::setValue(std::array<float, 4> v)
{
  m_value = v;
  auto hsv = QColor::fromRgbF(v[0], v[1], v[2], v[3]).toHsv();

  this->h = hsv.hueF();
  this->s = hsv.saturationF();
  this->v = hsv.valueF();
  update();
}

void QGraphicsHSVChooser::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  const auto p = event->pos();
  if (p.x() < 100.)
  {
    h = qBound(0., p.x() / 100., 1.);
    s = qBound(0., p.y() / 100., 1.);
    m_grab = true;
  }
  else if (p.x() >= 110 && p.x() < 130)
  {
    v = qBound(0., p.y() / 100., 1.);
    m_grab = true;
  }

  const auto rgba = QColor::fromHsvF(h, s, v, 1.);
  auto new_v = m_value;
  auto& [r, g, b, a] = new_v;
  r = rgba.redF();
  g = rgba.greenF();
  b = rgba.blueF();
  a = 1.;
  if (new_v != m_value)
  {
    m_value = new_v;
    valueChanged(m_value);
    sliderMoved();
    update();
  }
  event->accept();
}

void QGraphicsHSVChooser::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  const auto p = event->pos();
  if (m_grab)
  {
    if (p.x() < 100.)
    {
      h = qBound(0., p.x() / 100., 1.);
      s = qBound(0., p.y() / 100., 1.);
    }
    else if (p.x() >= 110 && p.x() < 130)
    {
      v = qBound(0., p.y() / 100., 1.);
    }

    const auto rgba = QColor::fromHsvF(h, s, v, 1.);
    auto new_v = m_value;
    auto& [r, g, b, a] = new_v;
    r = rgba.redF();
    g = rgba.greenF();
    b = rgba.blueF();
    a = 1.;
    if (new_v != m_value)
    {
      m_value = new_v;
      valueChanged(m_value);
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsHSVChooser::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    const auto p = event->pos();
    if (p.x() < 100.)
    {
      h = qBound(0., p.x() / 100., 1.);
      s = qBound(0., p.y() / 100., 1.);
    }
    else if (p.x() >= 110 && p.x() < 130)
    {
      v = qBound(0., p.y() / 100., 1.);
    }
    const auto rgba = QColor::fromHsvF(h, s, v, 1.);
    auto new_v = m_value;
    auto& [r, g, b, a] = new_v;
    r = rgba.redF();
    g = rgba.greenF();
    b = rgba.blueF();
    a = 1.;
    if (new_v != m_value)
    {
      m_value = new_v;
      valueChanged(m_value);
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  event->accept();
}

QRectF QGraphicsHSVChooser::boundingRect() const
{
  return QRectF{0, 0, 140, 100};
}

QGraphicsXYChooser::QGraphicsXYChooser(QGraphicsItem* parent) { }

void QGraphicsXYChooser::setPoint(const QPointF& r)
{
  SCORE_TODO;
}

void QGraphicsXYChooser::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  painter->fillRect(QRectF{0, 0, 100, 100}, score::Skin::instance().Dark.main.brush);

  auto x = m_value[0] * 100.;
  auto y = m_value[1] * 100.;

  painter->setPen(score::Skin::instance().DarkGray.main.pen0);
  painter->drawLine(QPointF{x, 0.}, QPointF{x, 100.});
  painter->drawLine(QPointF{0, y}, QPointF{100., y});
}

std::array<float, 2> QGraphicsXYChooser::value() const
{
  return m_value;
}

void QGraphicsXYChooser::setValue(std::array<float, 2> v)
{
  m_value = v;
  update();
}

void QGraphicsXYChooser::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  const auto p = event->pos();
  float newX = qBound(0., p.x() / 100., 1.);
  float newY = qBound(0., p.y() / 100., 1.);
  m_grab = true;

  ossia::vec2f newValue{newX, newY};
  if (m_value != newValue)
  {
    m_value = newValue;
    valueChanged(m_value);
    sliderMoved();
    update();
  }
  event->accept();
}

void QGraphicsXYChooser::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    const auto p = event->pos();
    float newX = qBound(0., p.x() / 100., 1.);
    float newY = qBound(0., p.y() / 100., 1.);
    m_grab = true;

    ossia::vec2f newValue{newX, newY};
    if (m_value != newValue)
    {
      m_value = newValue;
      valueChanged(m_value);
      sliderMoved();
      update();
    }
  }
  event->accept();
}

void QGraphicsXYChooser::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  if (m_grab)
  {
    const auto p = event->pos();
    float newX = qBound(0., p.x() / 100., 1.);
    float newY = qBound(0., p.y() / 100., 1.);
    m_grab = true;

    ossia::vec2f newValue{newX, newY};
    if (m_value != newValue)
    {
      m_value = newValue;
      valueChanged(m_value);
      update();
    }
    sliderReleased();
    m_grab = false;
  }
  event->accept();
}

QRectF QGraphicsXYChooser::boundingRect() const
{
  return QRectF{0, 0, 100, 100};
}

QGraphicsCheckBox::QGraphicsCheckBox( QGraphicsItem* parent)
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorPointingHand);
}

void QGraphicsCheckBox::toggle()
{
  m_toggled = !m_toggled;
}

void QGraphicsCheckBox::setState(bool toggled)
{
  if (toggled != m_toggled)
  {
    m_toggled = toggled;
  }
}

void QGraphicsCheckBox::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  m_toggled = !m_toggled;
  toggled(m_toggled);
  event->accept();
}

void QGraphicsCheckBox::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsCheckBox::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  event->accept();
}

void QGraphicsCheckBox::paint(
    QPainter* painter,
    const QStyleOptionGraphicsItem* option,
    QWidget* widget)
{
  auto& skin = score::Skin::instance();
  painter->setRenderHint(QPainter::Antialiasing, true);

  constexpr const double checkBoxWidth = 12.;
  constexpr const double insideBoxWidth = 6.;

  double positionCheckBox = (m_rect.width() - checkBoxWidth) * 0.5;
  painter->fillRect(QRectF{positionCheckBox,positionCheckBox, checkBoxWidth, checkBoxWidth}, skin.Emphasis2.main.brush);

  if(m_toggled)
  {
    double position = (m_rect.width() - insideBoxWidth) * 0.5;
    painter->setPen(skin.Base4.main.pen2);
    painter->drawLine(position, position, position + insideBoxWidth, position + + insideBoxWidth);
    painter->drawLine(position, position + + insideBoxWidth, position + + insideBoxWidth, position);
  //  painter->fillRect(QRectF{position,position, insideBoxWidth, insideBoxWidth}, skin.Base4.main.brush);
  }
 }

QRectF QGraphicsCheckBox::boundingRect() const
{
  return m_rect;
}

}

W_OBJECT_IMPL(score::ComboBoxWithEnter)
