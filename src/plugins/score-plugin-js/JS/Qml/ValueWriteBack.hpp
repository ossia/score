#pragma once

// Writing a port's or an address's value back into the QML property that is
// bound to it, without taking an edit away from the user.
//
// PortSource and AddressSource are two-way: the bound property drives the port
// (or the device address), and the port drives the property back. The property
// is very often the `text` of a TextField the user types into, and writing to
// it unconditionally is destructive in two different ways.
//
//  * The value coming back is frequently the one the property already holds --
//    the echo of the UI's own write. Writing it again is not a no-op for an
//    editor: it resets the cursor and the selection, and re-runs every binding
//    and onXChanged handler attached to the property.
//
//  * While the score plays, avnd republishes a snapshot of *every* control
//    input of a process as soon as any one of them changed: finish_run()
//    enqueues make_controls_in_tuple() whenever inputs_set.any()
//    (3rdparty/avendish, binding/ossia/node.hpp), and the UI side pushes the
//    whole tuple back through ControlInlet::setExecutionValue()
//    (score-plugin-avnd, Crousti/ExecutorUpdateControlValueInUi.hpp). That
//    snapshot is the executor's state, so it is one tick behind the keystroke
//    that has not reached the executor yet. Writing it back replaces what the
//    user sees with the pre-keystroke text: type one character into a playing
//    document and it disappears.
//
// The second case is the one an equality test cannot catch, precisely because
// the lagging snapshot differs from what the editor shows. What tells a stale
// echo of the user's own typing from a genuine external write is the keyboard
// focus: while the bound item has the active focus, its contents belong to the
// user. So both tests are needed, and they cover different ground:
//
//  * the equality test suppresses the writes that would change nothing but
//    still disturb the editor -- including on an item nobody is focused on;
//  * the focus test suppresses the writes that would change what the user is
//    in the middle of typing.
//
// Everything else still lands: an editor the user is not typing into follows
// its port, which is what makes an OSC or score-side write show up in the UI.
// A value suppressed because the item was focused is dropped rather than
// replayed on focus-out: by then it is older than the user's own edit, and
// applying it would clobber the very text this guard exists to protect. The
// port already holds what the user typed -- every keystroke is pushed to it by
// the notify-signal handler.
//
// score's own comment box arrives at the same place by construction: its
// Process::ControlInlet::executionValueChanged handler only calls update(), and
// the editor's document is only ever rewritten from the *model* value, with an
// equality test (src/plugins/score-plugin-ui/Ui/TextBox.hpp).

#include <QMetaType>
#include <QObject>
#include <QQmlProperty>
#include <QVariant>

namespace JS
{

//! Whether the user currently holds the keyboard inside the bound item.
/**
 * QQuickItem exposes this as the `activeFocus` property: focused *and* in the
 * active focus chain of an active window, which is exactly "the keystrokes are
 * going here". It is read through the metaobject rather than by casting to
 * QQuickItem, so a target that is not a QtQuick item -- a plain QtObject
 * property, a QWidget, a build without QtQuick -- simply never claims the
 * focus instead of failing to compile or to resolve.
 */
inline bool targetHasActiveFocus(const QQmlProperty& prop) noexcept
{
  auto* obj = prop.object();
  if(!obj)
    return false;

  const QVariant focus = obj->property("activeFocus");
  return focus.typeId() == QMetaType::Bool && focus.toBool();
}

//! Writes \p value into \p prop unless that would disturb an edit in progress.
/** \return whether the property was written. */
inline bool writeBackUnlessEditing(QQmlProperty& prop, const QVariant& value)
{
  if(!prop.isValid())
    return false;

  // Qt 6 compares numbers across int / float / double here, so a float port
  // echoing into a `real` property does not count as a change.
  if(prop.read() == value)
    return false;

  if(targetHasActiveFocus(prop))
    return false;

  prop.write(value);
  return true;
}

}
