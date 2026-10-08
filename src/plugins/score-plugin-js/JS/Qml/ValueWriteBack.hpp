#pragma once

// Writes a port's value back into its bound QML property without clobbering an
// in-progress edit. Two guards: equality, since the value is usually the echo of
// the UI's own write; and focus, because avnd republishes every control input of
// a process when any one changes, so the snapshot can lag a keystroke that has
// not reached the executor yet. A value suppressed by the focus guard is dropped
// rather than replayed, being older than what the port already holds.

#include <QMetaType>
#include <QObject>
#include <QQmlProperty>
#include <QVariant>

namespace JS
{

//! Whether the keystrokes are currently going into the bound item.
//! Read through the metaobject rather than by casting to QQuickItem, so a target
//! that is not a QtQuick item simply never claims focus.
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
