#pragma once
#include <QJSEngine>
#include <QJSManagedValue>
#include <QJSValue>
#include <QVariant>

#include <type_traits>

namespace JS
{
//! QJSEngine::toScriptValue(QVariant) turns a list nested in a map or a list
//! into a sequence wrapper, for which Array.isArray() is false: scripts that
//! check it (JSON-like messages from a custom UI, a script's state) drop the
//! data. Lists and maps are built as real arrays and objects instead.
//! Containers are read in place and scalars built directly: the rest of the
//! cost is the engine creating the objects.
inline QJSValue variantToJs(QJSEngine& engine, const QVariant& v);

namespace detail
{
template <typename Map>
inline QJSValue mapToJs(QJSEngine& engine, const Map& map)
{
  QJSManagedValue obj(engine.newObject(), &engine);
  for(auto it = map.cbegin(); it != map.cend(); ++it)
    obj.setProperty(it.key(), variantToJs(engine, it.value()));
  return obj.toJSValue();
}

template <typename List>
inline QJSValue listToJs(QJSEngine& engine, const List& list)
{
  QJSManagedValue arr(engine.newArray(uint(list.size())), &engine);
  for(qsizetype i = 0; i < list.size(); i++)
  {
    if constexpr(std::is_same_v<typename List::value_type, QString>)
      arr.setProperty(quint32(i), QJSValue(list[i]));
    else
      arr.setProperty(quint32(i), variantToJs(engine, list[i]));
  }
  return arr.toJSValue();
}

template <typename T>
inline const T& get(const QVariant& v) noexcept
{
  return *static_cast<const T*>(v.constData());
}
}

inline QJSValue variantToJs(QJSEngine& engine, const QVariant& v)
{
  switch(v.metaType().id())
  {
    case QMetaType::Double:
      return QJSValue(detail::get<double>(v));
    case QMetaType::Int:
      return QJSValue(detail::get<int>(v));
    case QMetaType::Bool:
      return QJSValue(detail::get<bool>(v));
    case QMetaType::QString:
      return QJSValue(detail::get<QString>(v));
    case QMetaType::QVariantList:
      return detail::listToJs(engine, detail::get<QVariantList>(v));
    case QMetaType::QStringList:
      return detail::listToJs(engine, detail::get<QStringList>(v));
    case QMetaType::QVariantMap:
      return detail::mapToJs(engine, detail::get<QVariantMap>(v));
    case QMetaType::QVariantHash:
      return detail::mapToJs(engine, detail::get<QVariantHash>(v));
    default:
      // A value from another engine (a script calling uiToExecution itself)
      // cannot be handed over as is.
      if(v.metaType() == QMetaType::fromType<QJSValue>())
        return variantToJs(engine, detail::get<QJSValue>(v).toVariant());
      return engine.toScriptValue(v);
  }
}
}
