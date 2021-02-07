#include <score/graphics/widgets/QGraphicsCombo.hpp>
#include <score/tools/Cursor.hpp>

#include <ossia/detail/math.hpp>

#include <QGraphicsSceneMouseEvent>
#include <QApplication>
#include <QScreen>
#include <QPainter>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::QGraphicsCombo);

namespace score
{
/*
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

  void focusInEvent(QFocusEvent* event) override
  {
    QComboBox::focusInEvent(event);
  }

  void focusOutEvent(QFocusEvent* event) override
  {
    dumpObjectTree();
    auto lvs = findChildren<QListView*>();

    if (!lvs.empty())
    {
      auto lv = lvs.front();
      lv->installEventFilter(this);
      QTimer::singleShot(2000, this, [this] {
        auto lvs = findChildren<QListView*>();
        if(!lvs.empty())
        {
          auto lv = lvs.front();
          if (!lv->hasFocus())
          {
            editingFinished();
          }
        }
      });
    }
    else
    {
      editingFinished();
    }
    QComboBox::focusOutEvent(event);
  }
};
*/
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
    {/*
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
      });*/
    }
    event->accept();
  }
};

QGraphicsCombo::QGraphicsCombo(QGraphicsItem* parent) : QGraphicsItem{parent}
{
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorSpin);
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
  auto& skin = score::Skin::instance();
  setCursor(skin.CursorSpin);
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
  painter->setBrush(skin.Emphasis2.main.brush);

  // Draw rect
  const QRectF brect = boundingRect().adjusted(1, 1, -1, -1);
  painter->drawRoundedRect(brect, 1, 1);

  // Draw text
  painter->setPen(skin.Base4.main.pen2);
  painter->setRenderHint(QPainter::Antialiasing, false);
  painter->setFont(skin.Medium10Pt);
  painter->drawText(brect, array[value()], QTextOption(Qt::AlignCenter));
}
}
