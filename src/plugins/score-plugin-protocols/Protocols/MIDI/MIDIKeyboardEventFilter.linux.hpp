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
  {
    keys.reserve(20);
  }

  bool eventFilter(QObject* object, QEvent* event)
  {
    if(event->type() == QEvent::KeyPress)
    {
      QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
      if(keyEvent->isAutoRepeat())
        return false;
      int k = key(*keyEvent);
      if(!keys.contains(k)) {
        keys.insert(k);
        press(k);
      }
    }
    else if(event->type() == QEvent::KeyRelease)
    {
      QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
      if(keyEvent->isAutoRepeat())
        return false;
      int k = key(*keyEvent);
      if (auto it = keys.find(k); it != keys.end()) {
        keys.erase(it);
        release(k);
      }
    }
    else if (event->type() == QEvent::ApplicationDeactivate) {
      for(auto k : keys)
        release(k);
      keys.clear();
    }
    return false;
  }

  inline uint32_t key(QKeyEvent& e)
  {
#if defined(__linux__)
    // X11 quirk
    return e.nativeScanCode() - scanCodeOffset;
#elif defined(__APPLE__)
    return e.nativeVirtualKey();
#else
    return e.nativeScanCode();
#endif
  }

  ossia::flat_set<int> keys;

#if defined(__linux__)
  const int scanCodeOffset = (qApp->platformName() == "xcb") ? -8 : 0;
#endif

};
}
#endif
