#pragma once

// Writes a port's value back into the QML property bound to it, without taking
// an edit away from the user.
//
// Two guards, covering different ground:
//  * equality -- the value is usually the echo of the UI's own write, and
//    rewriting it resets an editor's cursor and selection and re-runs its
//    bindings;
//  * focus -- while the score plays, avnd republishes every control input of a
//    process whenever any one changed (avendish binding/ossia/node.hpp ->
//    Crousti/ExecutorUpdateControlValueInUi.hpp), so the snapshot lags a
//    keystroke that has not reached the executor. Equality cannot catch that,
//    because the stale value genuinely differs from what the editor shows.
//
// A value suppressed by the focus guard is dropped, not replayed on focus-out:
// by then it is older than the user's edit, and the port already holds what was
// typed. Unfocused editors still follow their port, which is what makes an OSC
// or score-side write visible.
//
// Ui/TextBox.hpp reaches the same place by construction: it only ever rewrites
// from the model value, with an equality test.

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
