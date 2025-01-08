#pragma once
#if defined(__APPLE__)
#include <QDebug>
#include <QObject>

#include <CoreGraphics/CoreGraphics.h>
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
    CGEventMask mask = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp)
                       | CGEventMaskBit(kCGEventScrollWheel)
                       | CGEventMaskBit(kCGEventLeftMouseDown)
                       | CGEventMaskBit(kCGEventRightMouseDown);
    auto eventTap = CGEventTapCreate(
        kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, mask,
        [](CGEventTapProxy, CGEventType type, CGEventRef event, void* ctx) {
      auto& self = *decltype(this)(ctx);
      switch(type)
      {
        case kCGEventKeyDown:
          self.press(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
          break;
        case kCGEventKeyUp:
          self.release(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
          break;
      }
      return event;
    }, this);
    if(eventTap == nullptr)
      return;

    CFRunLoopAddSource(
        CFRunLoopGetCurrent(),
        CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0),
        kCFRunLoopCommonModes);
    // Enable the event tap.
    CGEventTapEnable(eventTap, true);
  }
};
}

#endif
