#pragma once

// Keyboard input for widget tests. score builds against Qt configurations that
// do not ship QtTest, and the suite standardizes on Catch2, so the few key
// presses the widget tests need are synthesized here instead.

#include <QCoreApplication>
#include <QKeyEvent>
#include <QString>
#include <QWidget>

namespace score::test
{

inline void keyEvent(
    QWidget& widget, QEvent::Type type, Qt::Key key, Qt::KeyboardModifiers mods,
    const QString& text)
{
  QKeyEvent ev{type, key, mods, text};
  QCoreApplication::sendEvent(&widget, &ev);
}

//! Press and release a single key on \p widget.
inline void keyClick(
    QWidget& widget, Qt::Key key, Qt::KeyboardModifiers mods = Qt::NoModifier,
    const QString& text = {})
{
  keyEvent(widget, QEvent::KeyPress, key, mods, text);
  keyEvent(widget, QEvent::KeyRelease, key, mods, text);
}

//! Type \p text on \p widget, one character at a time.
inline void keyClicks(
    QWidget& widget, const QString& text, Qt::KeyboardModifiers mods = Qt::NoModifier)
{
  for(const QChar c : text)
  {
    // Qt::Key_A .. Qt::Key_Z and Qt::Key_0 .. Qt::Key_9 are the upper-cased
    // code points; anything else keeps its own, which is what the widgets that
    // only look at text() need.
    const auto key = static_cast<Qt::Key>(c.toUpper().unicode());
    keyClick(widget, key, mods, QString{c});
  }
}

}
