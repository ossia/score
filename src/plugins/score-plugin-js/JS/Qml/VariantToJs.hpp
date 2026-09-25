#pragma once
#include <QJSEngine>
#include <QJSValue>
#include <QVariant>

namespace JS
{
//! QJSEngine::toScriptValue(QVariant) turns a list nested in a map or a list
//! into a sequence wrapper, for which Array.isArray() is false: scripts that
//! check it (JSON-like messages from a custom UI, a script's state) drop the
//! data. Lists and maps are built as real arrays and objects instead.
inline QJSValue variantToJs(QJSEngine& engine, const QVariant& v)
{
  // A value from another engine (a script calling uiToExecution itself)
  // cannot be handed over as is.
  if(v.metaType() == QMetaType::fromType<QJSValue>())
    return variantToJs(engine, v.value<QJSValue>().toVariant());

  switch(v.metaType().id())
  {
    case QMetaType::QVariantList:
    case QMetaType::QStringList: {
      const auto list = v.toList();
      auto arr = engine.newArray(list.size());
      for(qsizetype i = 0; i < list.size(); i++)
        arr.setProperty(quint32(i), variantToJs(engine, list[i]));
      return arr;
    }
    case QMetaType::QVariantMap: {
      const auto map = v.toMap();
      auto obj = engine.newObject();
      for(auto it = map.cbegin(); it != map.cend(); ++it)
        obj.setProperty(it.key(), variantToJs(engine, it.value()));
      return obj;
    }
    case QMetaType::QVariantHash: {
      const auto hash = v.toHash();
      auto obj = engine.newObject();
      for(auto it = hash.cbegin(); it != hash.cend(); ++it)
        obj.setProperty(it.key(), variantToJs(engine, it.value()));
      return obj;
    }
    default:
      return engine.toScriptValue(v);
  }
}
}
