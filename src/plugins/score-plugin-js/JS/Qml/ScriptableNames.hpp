#pragma once
#include <LocalTree/ScriptableProcessComponent.hpp>

#include <QJSValue>
#include <QObject>

#include <score_plugin_js_export.h>

#include <optional>
#include <verdigris>

namespace JS
{
//! Names published under score:/controls, triggers and conditions.
//! Reads only the given snapshot, so it is safe on the script's thread.
class SCORE_PLUGIN_JS_EXPORT ScriptableNames : public QObject
{
  W_OBJECT(ScriptableNames)
public:
  //! If names is null, the tree of the current document is read
  explicit ScriptableNames(
      const std::shared_ptr<const LocalTree::ScriptableSnapshot>* names,
      QObject* parent = nullptr);
  ~ScriptableNames();

  //! "node", "impulse", "value", or empty if the path does not exist
  QString kind(const QString& ns, const QString& path);
  W_SLOT(kind)
  QStringList names(const QString& ns, const QString& path);
  W_SLOT(names)
  QString address(const QString& ns, const QString& path);
  W_SLOT(address)

  //! Controls, Triggers and Conditions proxies over the given Device object
  static QJSValue
  makeNamespaces(QJSEngine& engine, const QJSValue& device, const QJSValue& names);

  //! Engine global holding the namespaces of the running process script
  static constexpr auto current = "__scoreScriptable";
  //! Proxy to namespace ns of the running script, for qualified Score imports
  static QJSValue makeSingleton(QJSEngine& engine, const QString& ns);

private:
  std::optional<LocalTree::ScriptableSnapshot::Entry>
  lookup(const QString& ns, const QString& path) const;

  const std::shared_ptr<const LocalTree::ScriptableSnapshot>* m_names{};
};
}
