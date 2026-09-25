#include "ScriptableNames.hpp"

#include <LocalTree/LocalTreeDocumentPlugin.hpp>
#include <LocalTree/ScriptableProcessComponent.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/base/parameter.hpp>

#include <QJSEngine>

#include <wobjectimpl.h>

W_OBJECT_IMPL(JS::ScriptableNames)

namespace JS
{
namespace
{
LocalTree::DocumentPlugin* currentLocalTree()
{
  auto doc = score::GUIAppContext().currentDocument();
  return doc ? doc->findPlugin<LocalTree::DocumentPlugin>() : nullptr;
}

// Structure is read through Names and values through Device, so the same
// code serves the console and processes, which have different Device objects.
constexpr auto prelude = R"_(
(function(Device, Names) {
  function make(ns, path) {
    const sub = (key) => path ? path + "/" + key : key;
    return new Proxy({}, {
      get(_, key) {
        if(typeof key !== "string")
          return undefined;
        if(key === "names")
          return () => Names.names(ns, path);
        if(key === "address" || key === "toString")
          return () => Names.address(ns, path);
        switch(Names.kind(ns, sub(key))) {
          case "node": return make(ns, sub(key));
          case "impulse": return () => Device.write(Names.address(ns, sub(key)), 1);
          case "value": return Device.read(Names.address(ns, sub(key)));
          default: return undefined;
        }
      },
      set(_, key, value) {
        Device.write(Names.address(ns, sub(key)), value);
        return true;
      },
      has(_, key) {
        return typeof key === "string" && Names.kind(ns, sub(key)) !== "";
      }
    });
  }
  return {
    Controls: make("controls", ""),
    Triggers: make("triggers", ""),
    Conditions: make("conditions", "")
  };
})
)_";
}

ScriptableNames::ScriptableNames(
    const std::shared_ptr<const LocalTree::ScriptableSnapshot>* names, QObject* parent)
    : QObject{parent}
    , m_names{names}
{
}

ScriptableNames::~ScriptableNames() = default;

static const LocalTree::ScriptableSnapshot::Entry*
entry(const LocalTree::ScriptableSnapshot* snap, const QString& ns, const QString& path)
{
  if(!snap)
    return nullptr;
  auto it = snap->entries.find(path.isEmpty() ? ns : ns + '/' + path);
  return it != snap->entries.end() ? &it->second : nullptr;
}

// The console may query before the snapshot is rebuilt, so it reads the tree;
// processes run on the execution thread and read their snapshot.
std::optional<LocalTree::ScriptableSnapshot::Entry>
ScriptableNames::lookup(const QString& ns, const QString& path) const
{
  if(m_names)
  {
    if(auto e = entry(m_names->get(), ns, path))
      return *e;
    return std::nullopt;
  }

  auto plug = currentLocalTree();
  if(!plug)
    return std::nullopt;
  const auto full = path.isEmpty() ? ns : ns + '/' + path;
  auto node = ossia::net::find_node(plug->device().get_root_node(), full.toStdString());
  if(!node || ossia::net::get_zombie(*node))
    return std::nullopt;

  LocalTree::ScriptableSnapshot::Entry e;
  e.address = LocalTree::addressOfNode(*node).toString();
  if(auto p = node->get_parameter())
    e.kind = p->get_value_type() == ossia::val_type::IMPULSE ? QStringLiteral("impulse")
                                                             : QStringLiteral("value");
  else
    e.kind = QStringLiteral("node");
  for(auto child : node->children_copy())
    if(!ossia::net::get_zombie(*child))
      e.children.push_back(QString::fromStdString(child->get_name()));
  return e;
}

QString ScriptableNames::kind(const QString& ns, const QString& path)
{
  if(auto e = lookup(ns, path))
    return e->kind;
  return {};
}

QStringList ScriptableNames::names(const QString& ns, const QString& path)
{
  if(auto e = lookup(ns, path))
    return e->children;
  return {};
}

QString ScriptableNames::address(const QString& ns, const QString& path)
{
  if(auto e = lookup(ns, path))
    return e->address;
  return {};
}

QJSValue ScriptableNames::makeNamespaces(
    QJSEngine& engine, const QJSValue& device, const QJSValue& names)
{
  auto fn = engine.evaluate(QString::fromUtf8(prelude));
  return fn.call({device, names});
}

QJSValue ScriptableNames::makeSingleton(QJSEngine& engine, const QString& ns)
{
  auto fn = engine.evaluate(QStringLiteral(R"_(
(function(name, key, global) {
  const ns = () => { const c = global[key]; return c ? c[name] : undefined; };
  return new Proxy({}, {
    get(_, k) { const n = ns(); return n ? n[k] : undefined; },
    set(_, k, v) { const n = ns(); if(n) n[k] = v; return true; },
    has(_, k) { const n = ns(); return n ? k in n : false; }
  });
})
)_"));
  return fn.call({ns, QString::fromLatin1(current), engine.globalObject()});
}
}
