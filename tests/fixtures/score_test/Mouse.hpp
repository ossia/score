#pragma once

// Mouse input for widget tests, the companion of Keyboard.hpp: score builds
// against Qt configurations that do not ship QtTest, so the clicks the widget
// tests need are synthesized here rather than through QTest::mouseClick.

#include <QApplication>
#include <QMouseEvent>
#include <QWidget>
#include <QWindow>

namespace score::test
{

/**
 * @brief Delivers a mouse event to \p widget the way the platform would.
 *
 * Through the window, not straight at the widget: QWidgetWindow is where Qt
 * decides which widget the click gives the focus to, and a test that sends the
 * event to the target itself gets the press without the focus change. That is
 * a real difference for a widget embedded in a QGraphicsProxyWidget, where the
 * focus is the whole subject.
 */
inline void mouseEvent(
    QWidget& widget, QEvent::Type type, QPoint pos, Qt::MouseButton button,
    Qt::MouseButtons buttons, Qt::KeyboardModifiers mods)
{
  const QPoint global = widget.mapToGlobal(pos);
  QWidget* window = widget.window();
  QWindow* handle = window ? window->windowHandle() : nullptr;

  if(!handle)
  {
    QMouseEvent ev{type, QPointF(pos), QPointF(global), button, buttons, mods};
    QCoreApplication::sendEvent(&widget, &ev);
    return;
  }

  const QPointF inWindow = widget.mapTo(window, pos);
  QMouseEvent ev{type, inWindow, inWindow, QPointF(global), button, buttons, mods};
  QCoreApplication::sendEvent(handle, &ev);
}

//! Press and release \p button at \p pos, in \p widget's coordinates.
inline void mouseClick(
    QWidget& widget, QPoint pos, Qt::MouseButton button = Qt::LeftButton,
    Qt::KeyboardModifiers mods = Qt::NoModifier)
{
  mouseEvent(widget, QEvent::MouseButtonPress, pos, button, button, mods);
  mouseEvent(widget, QEvent::MouseButtonRelease, pos, button, Qt::NoButton, mods);
  QApplication::processEvents();
}

/**
 * @brief Shows \p w and waits for it to become the active window.
 *
 * Qt only delivers focus events inside an active window, and there is no
 * window manager on a CI display (nor under the offscreen platform), so
 * activation is a round trip rather than something show() already did.
 *
 * False when the platform never activates it: a test that turns on focus
 * should skip rather than assert.
 */
inline bool showAndActivate(QWidget& w)
{
  w.show();
  QApplication::processEvents();
  w.raise();
  w.activateWindow();
  for(int i = 0; i < 200 && !w.isActiveWindow(); i++)
    QApplication::processEvents(QEventLoop::AllEvents, 10);
  return w.isActiveWindow();
}

}
