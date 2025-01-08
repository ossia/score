#pragma once
#if !defined(__APPLE__)
#include <score/application/GUIApplicationContext.hpp>

#include <QGuiApplication>
#include <QKeyEvent>
#include <QMainWindow>
#include <QObject>

#include <libremidi/backends/keyboard/config.hpp>

namespace Protocols
{
class MidiKeyboardEventFilter : public QObject
{
public:
  libremidi::kbd_input_configuration::scancode_callback press, release;
  MidiKeyboardEventFilter(
      libremidi::kbd_input_configuration::scancode_callback press,
      libremidi::kbd_input_configuration::scancode_callback release)
      : press{std::move(press)}
      , release{std::move(release)}
      , target{score::GUIAppContext().mainWindow}
  {
  }

  bool eventFilter(QObject* object, QEvent* event)
  {
    if(object == target)
    {
      if(event->type() == QEvent::KeyPress)
      {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if(!keyEvent->isAutoRepeat())
          press(key(*keyEvent));
      }
      else if(event->type() == QEvent::KeyRelease)
      {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if(!keyEvent->isAutoRepeat())
          release(key(*keyEvent));
      }
    }
    return false;
  }

  inline uint32_t key(QKeyEvent& e)
  {
#if defined(__linux__)
    // X11 quirk
    static const int scanCodeOffset = (qApp->platformName() == "xcb") ? -8 : 0;
    return e.nativeScanCode() - scanCodeOffset;
#elif defined(__APPLE__)
    return e.nativeVirtualKey();
#else
    return e.nativeScanCode();
#endif
  }
  QObject* target{};
};
}
#endif
