#include "TypeInWidget.hpp"

#include <score/widgets/DoubleSpinBox.hpp>
#include <score/widgets/SignalUtils.hpp>

#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QTimer>

namespace score
{
namespace
{
/**
 * @brief Decides when the box a right-click raised goes away.
 *
 * Lives as a child of the proxy, so it dies with the box it watches.
 */
class TypeInGuard final : public QObject
{
public:
  TypeInGuard(QGraphicsProxyWidget& proxy, std::function<void()> onFinished)
      : QObject{&proxy}
      , m_proxy{proxy}
      , m_onFinished{std::move(onFinished)}
  {
  }

  //! An edit is pending: the release at the end of it is worth emitting.
  void touch() noexcept { m_dirty = true; }

  /**
   * @brief One field is done being typed into.
   *
   * Emitted several times per focus change -- DoubleSpinboxWithEnter emits it
   * on the focus-out and QAbstractSpinBox does so too -- hence the guard on an
   * edit having actually happened.
   */
  void finish()
  {
    // Enter is three editingFinished in a row -- the box's own handler, the
    // fall-through in it, and QAbstractSpinBox re-interpreting the text -- and
    // the re-interpretation sets the value again in between, so an edit being
    // pending does not tell them apart. Once a close has been asked for, the
    // first of them is the last release this box reports.
    if(m_done || !m_dirty)
      return;

    m_dirty = false;
    m_done = m_closing;
    if(m_onFinished)
      m_onFinished();
  }

  //! Take the box down unless the focus is still somewhere inside it.
  void closeIfFocusLeft()
  {
    auto* proxy = &m_proxy;
    QTimer::singleShot(0, proxy, [proxy] {
      if(currentRightClickWidget() != proxy || proxy->hasFocus())
        return;
      closeRightClickWidget();
    });
  }

  //! Enter and Escape are done with the box whatever still has the focus.
  void closeNow()
  {
    m_closing = true;
    auto* proxy = &m_proxy;
    QTimer::singleShot(0, proxy, [proxy] {
      if(currentRightClickWidget() == proxy)
        closeRightClickWidget();
    });
  }

private:
  // ShortcutOverride as well as KeyPress: Qt asks first whether anything wants
  // the key as a shortcut, and DoubleSpinboxWithEnter answers Enter there --
  // so the box has already reported the end of the edit by the time the key
  // press itself arrives.
  bool eventFilter(QObject* obj, QEvent* ev) override
  {
    if(ev->type() == QEvent::KeyPress || ev->type() == QEvent::ShortcutOverride)
    {
      switch(static_cast<QKeyEvent*>(ev)->key())
      {
        case Qt::Key_Enter:
        case Qt::Key_Return:
        case Qt::Key_Escape:
          // Not consumed: the box still has to commit what was typed, which it
          // does by emitting editingFinished from its own handler.
          closeNow();
          break;
        default:
          break;
      }
    }
    return QObject::eventFilter(obj, ev);
  }

  QGraphicsProxyWidget& m_proxy;
  std::function<void()> m_onFinished;
  bool m_dirty{};
  bool m_closing{};
  bool m_done{};
};
}

QGraphicsProxyWidget* showTypeInBox(
    QGraphicsScene& scene, QPointF scenePos, const std::vector<TypeInField>& fields,
    std::function<void(int, double)> onChanged, std::function<void()> onFinished)
{
  if(fields.empty())
    return nullptr;

  // Only one type-in box at a time, here as on the sliders.
  closeRightClickWidget();

  auto* holder = new QWidget;
  auto* lay = new QHBoxLayout{holder};
  lay->setContentsMargins(2, 2, 2, 2);
  lay->setSpacing(2);

  std::vector<DoubleSpinboxWithEnter*> boxes;
  boxes.reserve(fields.size());
  for(const TypeInField& f : fields)
  {
    auto* b = new DoubleSpinboxWithEnter;
    b->setRange(f.min, f.max);
    b->setDecimals(f.decimals);
    b->setValue(f.value);
    if(!f.prefix.isEmpty())
      b->setPrefix(f.prefix + ' ');
    lay->addWidget(b);
    boxes.push_back(b);
  }

  auto* proxy
      = scene.addWidget(holder, Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
  proxy->setPos(scenePos);
  currentRightClickWidget() = proxy;

  auto* guard = new TypeInGuard{*proxy, std::move(onFinished)};

  for(std::size_t i = 0; i < boxes.size(); i++)
  {
    auto* b = boxes[i];
    b->installEventFilter(guard);

    QObject::connect(
        b, SignalUtils::QDoubleSpinBox_valueChanged_double(), guard,
        [guard, changed = onChanged, i](double v) {
      guard->touch();
      if(changed)
        changed(int(i), v);
    });

    QObject::connect(b, &DoubleSpinboxWithEnter::editingFinished, guard, [guard] {
      guard->finish();
      guard->closeIfFocusLeft();
    });
  }

  QTimer::singleShot(0, boxes[0], [b = boxes[0]] {
    b->setFocus();
    b->selectAll();
  });

  return proxy;
}
}
