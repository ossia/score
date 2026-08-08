#include <score/graphics/InfiniteScroller.hpp>
#include <score/tools/Cursor.hpp>
#include <score/tools/PointerLock.hpp>

#include <QCoreApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPointer>
#include <QScreen>
#include <QTimer>
#include <QWidget>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>

#include <cmath>

namespace score
{
namespace
{
bool relativeSession{};
bool discardNextPointerDelta{};
QGraphicsItem* draggedItem{};
bool cursorHidden{};
bool usedPointerPositions{};
int silentLockedMoves{};

// A backend can report the lock as held and then deliver nothing -- a refused
// Wayland constraint, a platform that never sends the motion. A lock that is
// really held also keeps the pointer on the press point, so a pointer that has
// wandered off it while no motion arrives is one that is not held at all: give
// up on it rather than leave the control frozen for the rest of the drag.
constexpr int silentLockedMovesBeforeFallback = 3;
constexpr double escapedPointerDistance = 20.;
quint64 relativeGeneration{};
QPointer<QWindow> relativeWindow{};
QMetaObject::Connection relativeWindowGone{};
QPointF virtualPos{};

// Long enough for the release the platform sends through its normal channel.
constexpr int relativeReleaseGraceMs = 50;

// Only worth doing once the pointer is actually being held: a hidden cursor
// that then wanders off across the screen is harder to follow than a visible
// one, and where no lock is granted that is exactly what happens.
void hideCursorForHeldPointer()
{
  if(cursorHidden || !draggedItem)
    return;

  cursorHidden = true;
#if !defined(__EMSCRIPTEN__)
  draggedItem->setCursor(QCursor(Qt::BlankCursor));
#endif
}

void endRelativeSession()
{
  QObject::disconnect(relativeWindowGone);
  relativeWindowGone = QMetaObject::Connection{};

  if(relativeSession)
    PointerLock::endRelative();
  relativeSession = false;
  relativeWindow.clear();
  ++relativeGeneration;
}

QWindow* windowOf(QGraphicsItem& item) noexcept
{
  auto* scene = item.scene();
  if(!scene)
    return nullptr;

  for(auto* view : scene->views())
    if(auto* w = view->window()->windowHandle())
      return w;
  return nullptr;
}

// Last resort for a backend that ends the drag without the widget ever seeing
// a release: an item left grabbed is as bad as a pointer left locked. Deferred,
// so that a release the widget does get keeps going through the usual path.
void scheduleRelativeTeardown(int delayMs, bool injectRelease)
{
  const quint64 generation = relativeGeneration;
  QTimer::singleShot(delayMs, qGuiApp, [generation, injectRelease] {
    if(!relativeSession || relativeGeneration != generation)
      return;

    auto* win = relativeWindow.data();
    endRelativeSession();

    if(injectRelease && win)
      QWindowSystemInterface::handleMouseEvent(
          win, win->mapFromGlobal(virtualPos), virtualPos, Qt::NoButton, Qt::LeftButton,
          QEvent::MouseButtonRelease, qGuiApp->keyboardModifiers());
  });
}

// QGuiApplication drops mouse moves that do not change the cursor position,
// which a locked pointer never does.
void onRelativeMotion(QPointF delta)
{
  auto* win = relativeWindow.data();
  if(!win)
  {
    scheduleRelativeTeardown(0, false);
    return;
  }

  const QRectF geom = win->geometry();
  virtualPos += delta;
  virtualPos.setX(qBound(geom.left(), virtualPos.x(), geom.right()));
  virtualPos.setY(qBound(geom.top(), virtualPos.y(), geom.bottom()));

  const QPointF local = win->mapFromGlobal(virtualPos);
  QMouseEvent ev{
      QEvent::MouseMove,
      local,
      local,
      virtualPos,
      Qt::NoButton,
      qGuiApp->mouseButtons(),
      qGuiApp->keyboardModifiers()};
  QCoreApplication::sendEvent(win, &ev);
}

void onRelativeRelease()
{
  scheduleRelativeTeardown(relativeReleaseGraceMs, true);
}

void watchForTeardown(QWindow* window)
{
  static const bool quitHandler = [] {
    QObject::connect(
        qGuiApp, &QCoreApplication::aboutToQuit, qGuiApp, [] { endRelativeSession(); });
    return true;
  }();
  Q_UNUSED(quitHandler);

  if(window)
  {
    relativeWindowGone = QObject::connect(
        window, &QObject::destroyed, qGuiApp, [] { endRelativeSession(); });
  }
}
}

QRectF InfiniteScroller::currentGeometry{};
double InfiniteScroller::origValue{};
double InfiniteScroller::currentSpeed{};
double InfiniteScroller::currentDelta{};

void InfiniteScroller::cancel()
{
  endRelativeSession();
  discardNextPointerDelta = false;
  usedPointerPositions = false;
  silentLockedMoves = 0;
  draggedItem = nullptr;
  cursorHidden = false;
}

void InfiniteScroller::start(QGraphicsItem& self, double orig)
{
  cancel();

  currentDelta = 0.;
  origValue = orig;
  draggedItem = &self;

  auto* screen = qGuiApp->screenAt(QCursor::pos());
  if(!screen)
    screen = qGuiApp->primaryScreen();
  currentGeometry = screen->availableGeometry();

  auto* window = windowOf(self);
  relativeWindow = window;
  virtualPos = QCursor::pos();
  relativeSession
      = PointerLock::beginRelative(window, &onRelativeMotion, &onRelativeRelease);
  if(relativeSession)
    watchForTeardown(window);

  // Backends that hold the pointer from the outset say so straight away; the
  // rest only prove it once motion arrives, and hide the cursor then.
  if(relativeSession && PointerLock::active())
    hideCursorForHeldPointer();
}

void InfiniteScroller::move_free(QGraphicsSceneMouseEvent* event)
{
  const double ratio = qGuiApp->keyboardModifiers() & Qt::CTRL ? 0.2 : 1.;

  // Decided per move rather than latched at start: the lock is granted
  // asynchronously and can be refused or dropped mid-drag, and the pointer
  // positions below still work when it is not held.
  if(relativeSession && PointerLock::active())
  {
    const double delta = ratio * PointerLock::takeDelta().y();
    const QPointF drift
        = event->screenPos() - event->buttonDownScreenPos(Qt::LeftButton);
    const bool escaped
        = std::abs(drift.x()) + std::abs(drift.y()) > escapedPointerDistance;
    if(delta == 0. && escaped
       && ++silentLockedMoves >= silentLockedMovesBeforeFallback)
    {
      endRelativeSession();
      discardNextPointerDelta = false;
    }
    else
    {
      if(delta != 0.)
        silentLockedMoves = 0;
      hideCursorForHeldPointer();
      currentSpeed = delta;
      currentDelta += delta;
      discardNextPointerDelta = true;
      return;
    }
  }

  // The pointer did not move while it was locked, but the synthesized moves did:
  // the first delta measured after a relative session is the whole drag, backwards.
  if(discardNextPointerDelta)
  {
    discardNextPointerDelta = false;
    currentSpeed = 0.;
    return;
  }

  usedPointerPositions = true;

  const double delta = (event->screenPos().y() - event->lastScreenPos().y());
  if(std::abs(delta) < 500)
  {
    currentSpeed = ratio * delta;
    currentDelta += ratio * delta;
  }

  const double top = currentGeometry.top();
  const double bottom = currentGeometry.bottom();
  const bool wrapUp = event->screenPos().y() <= top + 100;
  const bool wrapDown = !wrapUp && event->screenPos().y() >= bottom - 100;

  if(wrapUp)
  {
    score::setCursorPos(QPointF(event->screenPos().x(), bottom - 101));
  }
  else if(wrapDown)
  {
    score::setCursorPos(QPointF(event->screenPos().x(), top + 101));
  }
}

double InfiniteScroller::move(QGraphicsSceneMouseEvent* event)
{
  move_free(event);

  const double max = currentGeometry.height();
  double v = origValue - currentDelta / max;
  if(v <= 0.)
  {
    currentDelta = origValue * max;
    v = 0.;
  }
  else if(v >= 1.)
  {
    currentDelta = ((origValue - 1.) * max);
    v = 1.;
  }

  return v;
}

void InfiniteScroller::stop(QGraphicsItem& self, QGraphicsSceneMouseEvent* event)
{
  const bool restoreCursor = usedPointerPositions;
  const bool hidden = cursorHidden;
  cancel();

  if(restoreCursor)
    score::setCursorPos(event->buttonDownScreenPos(Qt::LeftButton));

  if(hidden)
    self.unsetCursor();
}
}
