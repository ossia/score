// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "GraphicsItem.hpp"

#include <score/graphics/ItemBounder.hpp>
#include <score/plugins/UuidKey.hpp>
#include <score/tools/Debug.hpp>
#include <ossia/detail/hash_map.hpp>

#include <QUrl>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QInputMethod>

#if defined(__EMSCRIPTEN__)
#include <QApplication>
#include <QStringList>
#include <QWindow>

#include <emscripten/em_asm.h>
#include <emscripten/emscripten.h>
#include <emscripten/val.h>

#include <cstdlib>
#include <cxxabi.h>
#include <string>
#include <typeinfo>
#endif

using item_help = ossia::hash_map<int, std::pair<QString, QUrl>>;
#if __has_include(<QApplicationStatic>)
#include <QApplicationStatic>
Q_APPLICATION_STATIC(item_help, g_itemHelpRegistry);
#else
Q_GLOBAL_STATIC(item_help, g_itemHelpRegistry);
#endif

void registerItemHelp(int itemType, QString tooltip, QUrl url) noexcept
{
  auto& val = *g_itemHelpRegistry;
  val[itemType] = std::pair<QString, QUrl>{tooltip, url};
}

QUrl getItemHelpUrl(int itemType) noexcept
{
  auto& val = *g_itemHelpRegistry;
  return val[itemType].second;
}

#if defined(__EMSCRIPTEN__)
namespace
{
QString imeDescribe(const QObject* obj)
{
  if(!obj)
    return QStringLiteral("<null>");
  QString res = QString::fromUtf8(obj->metaObject()->className());
  if(!obj->objectName().isEmpty())
    res += QStringLiteral("(\"%1\")").arg(obj->objectName());
  return res;
}

QString imeDescribe(const QGraphicsItem* item)
{
  if(!item)
    return QStringLiteral("<null>");

  QString res;
  if(auto* obj = const_cast<QGraphicsItem*>(item)->toGraphicsObject())
  {
    res = QString::fromUtf8(obj->metaObject()->className());
  }
  else
  {
    const char* mangled = typeid(*item).name();
    int status{};
    char* demangled = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    res = QString::fromUtf8(status == 0 && demangled ? demangled : mangled);
    std::free(demangled);
  }

  const auto flags = item->flags();
  res += QStringLiteral(" [itemType=%1 acceptsInputMethod=%2 focusable=%3]")
             .arg(item->type())
             .arg(bool(flags & QGraphicsItem::ItemAcceptsInputMethod) ? "yes" : "no")
             .arg(bool(flags & QGraphicsItem::ItemIsFocusable) ? "yes" : "no");
  return res;
}

void imeInstallJs()
{
  static bool installed = false;
  if(installed)
    return;
  installed = true;

  // clang-format off
  EM_ASM({
    globalThis.__scoreImeDom = function() {
      try {
        var describe = function(e) {
          if (!e) return "<null>";
          var s = e.tagName ? e.tagName.toLowerCase() : "<no-tag>";
          if (e.id) s += "#" + e.id;
          var cls = (typeof e.className === "string") ? e.className.trim() : "";
          if (cls) s += "." + cls.split(" ").filter(function(c) { return c.length; }).join(".");
          return s;
        };
        var mode = function(e) {
          if (!e || e.inputMode === undefined) return "<undefined>";
          return e.inputMode === "" ? "<empty>" : e.inputMode;
        };
        // Qt for wasm puts the canvases and the hidden <input> elements in a
        // shadow root, so document.activeElement only reports the host.
        var chain = [];
        var a = document.activeElement;
        while (a) {
          chain.push(describe(a));
          if (a.shadowRoot && a.shadowRoot.activeElement) a = a.shadowRoot.activeElement;
          else break;
        }
        var host = document.querySelector("#qt-shadow-container");
        var root = (host && host.shadowRoot) ? host.shadowRoot : document;
        var isQtInput = !!(a && a.matches && a.matches("input.qt-window-input-element"));
        var lines = [];
        lines.push("  dom.activeElement    : " + describe(a));
        lines.push("  dom.activeChain      : " + chain.join(" > "));
        lines.push("  dom.isQtInputElement : " + isQtInput);
        lines.push("  dom.activeInputMode  : " + mode(a));
        if (a) {
          lines.push("  dom.activeFlags      : readOnly=" + a.readOnly
                     + " disabled=" + a.disabled + " tabIndex=" + a.tabIndex
                     + " contentEditable=" + a.contentEditable);
        }
        lines.push("  dom.documentHasFocus : " + document.hasFocus());
        var wins = root.querySelectorAll(".qt-window");
        lines.push("  dom.qtWindows        : " + wins.length);
        for (var w = 0; w < wins.length; w++) {
          var win = wins[w];
          var t = win.querySelector(".title");
          lines.push("    win[" + w + "] title=\"" + (t ? t.textContent : "") + "\""
                     + " display=" + win.style.display
                     + " containsActive=" + win.contains(a));
        }
        var inputs = root.querySelectorAll("input.qt-window-input-element");
        lines.push("  dom.qtInputElements  : " + inputs.length);
        for (var i = 0; i < inputs.length; i++) {
          var e = inputs[i];
          var st = window.getComputedStyle(e);
          var r = e.getBoundingClientRect();
          lines.push("    [" + i + "] focused=" + (e === a)
                     + " container=" + describe(e.parentElement)
                     + " inputMode=" + mode(e)
                     + " valueLength=" + (e.value ? e.value.length : 0)
                     + " display=" + st.display
                     + " visibility=" + st.visibility
                     + " opacity=" + st.opacity
                     + " rect=" + Math.round(r.x) + "," + Math.round(r.y)
                     + " " + Math.round(r.width) + "x" + Math.round(r.height));
        }
        return lines.join("\n");
      } catch (err) {
        return "  dom: <error " + err + ">";
      }
    };

    var call = function() {
      var f = (typeof _score_ime_diag_text !== "undefined")
                ? _score_ime_diag_text
                : Module["_score_ime_diag_text"];
      return UTF8ToString(f());
    };

    globalThis.scoreImeDiag = function() {
      var s = call();
      console.log(s);
      return s;
    };

    globalThis.scoreImeLog = function(on) {
      var f = (typeof _score_ime_set_logging !== "undefined")
                ? _score_ime_set_logging
                : Module["_score_ime_set_logging"];
      f((on === undefined || on) ? 1 : 0);
      return (on === undefined || on) ? "score IME logging ON" : "score IME logging OFF";
    };
  });
  // clang-format on
}

bool imeLoggingRequested()
{
  if(qEnvironmentVariableIsSet("SCORE_IME_LOG"))
    return true;

  using emscripten::val;
  val loc = val::global("location");
  if(loc.isNull() || loc.isUndefined())
    return false;

  std::string url;
  for(const char* prop : {"search", "hash"})
  {
    val v = loc[prop];
    if(v.isString())
      url += v.as<std::string>();
  }
  return url.find("imelog") != std::string::npos;
}

bool& imeLoggingFlag()
{
  static bool enabled = [] {
    imeInstallJs();
    return imeLoggingRequested();
  }();
  return enabled;
}

QString imeDomSnapshot()
{
  imeInstallJs();
  using emscripten::val;
  val fun = val::global("__scoreImeDom");
  if(!fun.isNull() && !fun.isUndefined())
  {
    val res = fun();
    if(res.isString())
      return QString::fromStdString(res.as<std::string>());
  }
  return QStringLiteral("  dom: <unavailable>");
}

void imeConsoleLog(const QString& msg)
{
  emscripten::val::global("console").call<void>("log", msg.toStdString());
}

QString imeSnapshot()
{
  QStringList out;
  out << QStringLiteral("=== score IME diagnostics ===");

  auto* focus = QGuiApplication::focusObject();
  auto* im = QGuiApplication::inputMethod();

  out << QStringLiteral("  qt.focusObject       : ") + imeDescribe(focus);
  out << QStringLiteral("  qt.focusWindow       : ") + imeDescribe(QGuiApplication::focusWindow());
  out << QStringLiteral("  qt.focusWidget       : ") + imeDescribe(QApplication::focusWidget());
  out << QStringLiteral("  qt.activeWindow      : ") + imeDescribe(QApplication::activeWindow());

  if(focus)
  {
    QInputMethodQueryEvent query{Qt::ImEnabled | Qt::ImReadOnly | Qt::ImHints};
    QCoreApplication::sendEvent(focus, &query);
    out << QStringLiteral("  qt.ImEnabled         : %1")
               .arg(query.value(Qt::ImEnabled).toBool() ? "true" : "false");
    out << QStringLiteral("  qt.ImReadOnly        : %1")
               .arg(query.value(Qt::ImReadOnly).toBool() ? "true" : "false");
    out << QStringLiteral("  qt.ImHints           : 0x%1")
               .arg(query.value(Qt::ImHints).toUInt(), 0, 16);
  }

  if(im)
  {
    out << QStringLiteral("  qt.inputMethodVisible: %1").arg(im->isVisible() ? "true" : "false");
    out << QStringLiteral("  qt.cursorRectangle   : %1,%2 %3x%4")
               .arg(im->cursorRectangle().x())
               .arg(im->cursorRectangle().y())
               .arg(im->cursorRectangle().width())
               .arg(im->cursorRectangle().height());
  }

  for(auto* view : {qobject_cast<QGraphicsView*>(focus),
                    qobject_cast<QGraphicsView*>(QApplication::focusWidget())})
  {
    if(view && view->scene())
    {
      out << QStringLiteral("  qt.sceneFocusItem    : ") + imeDescribe(view->scene()->focusItem());
      break;
    }
  }

  out << imeDomSnapshot();
  return out.join('\n');
}
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* score_ime_diag_text()
{
  static std::string buf;
  buf = imeSnapshot().toStdString();
  return buf.c_str();
}

extern "C" EMSCRIPTEN_KEEPALIVE void score_ime_set_logging(int on)
{
  imeLoggingFlag() = (on != 0);
}
#endif

namespace score
{
void retargetInputMethod(const char* context) noexcept
{
#if defined(__EMSCRIPTEN__)
  const bool log = imeLoggingFlag();
  auto* focus = QGuiApplication::focusObject();
  auto* im = QGuiApplication::inputMethod();
  if(!focus || !im)
  {
    if(log)
      imeConsoleLog(
          QStringLiteral("[ime] retarget(%1) -> bail: no focus object / input method\n%2")
              .arg(QString::fromUtf8(context), imeDomSnapshot()));
    return;
  }

  QInputMethodQueryEvent query{Qt::ImEnabled};
  QCoreApplication::sendEvent(focus, &query);
  const bool enabled = query.value(Qt::ImEnabled).toBool();

  if(log)
    imeConsoleLog(QStringLiteral("[ime] retarget(%1) focusObject=%2 ImEnabled=%3 -> %4\n%5")
                      .arg(
                          QString::fromUtf8(context), imeDescribe(focus),
                          enabled ? "true" : "false", enabled ? "show()" : "bail",
                          imeDomSnapshot()));

  if(!enabled)
    return;

  im->update(Qt::ImEnabled | Qt::ImQueryInput);
  im->show();

  if(log)
    imeConsoleLog(QStringLiteral("[ime] retarget(%1) after show()\n%2")
                      .arg(QString::fromUtf8(context), imeDomSnapshot()));
#else
  (void)context;
#endif
}

void watchSceneInputMethod(QGraphicsScene& scene)
{
#if defined(__EMSCRIPTEN__)
  imeInstallJs();
  QObject::connect(
      &scene, &QGraphicsScene::focusItemChanged, &scene,
      [](QGraphicsItem* newItem, QGraphicsItem* oldItem, Qt::FocusReason reason) {
    const bool accepts
        = newItem && (newItem->flags() & QGraphicsItem::ItemAcceptsInputMethod);
    if(imeLoggingFlag())
      imeConsoleLog(QStringLiteral("[ime] focusItemChanged reason=%1\n  new: %2\n  old: %3")
                        .arg(int(reason))
                        .arg(imeDescribe(newItem), imeDescribe(oldItem)));
    if(!accepts)
      return;
    retargetInputMethod("focusItemChanged");
  });
#else
  (void)scene;
#endif
}
}

void deleteGraphicsObject(QGraphicsObject* item)
{
  if(item)
  {
    auto sc = item->scene();

    if(sc)
    {
      sc->removeItem(item);
    }

    item->deleteLater();
  }
}

void deleteGraphicsItem(QGraphicsItem* item)
{
  if(item)
  {
    auto sc = item->scene();

    if(sc)
    {
      sc->removeItem(item);
    }

    delete item;
  }
}

QGraphicsView* getView(const QGraphicsItem& self)
{
  if(!self.scene())
    return nullptr;
  auto v = self.scene()->views();
  if(v.empty())
    return nullptr;
  return v.first();
}

// TODO apparently crashes on macOS... investigate
QGraphicsView* getView(const QPainter& painter)
{
  auto widg = static_cast<QWidget*>(painter.device());
  SCORE_ASSERT(widg);
  return static_cast<QGraphicsView*>(widg->parent());
}

QImage newImage(double logical_w, double logical_h)
{
  double ratio = qGuiApp->devicePixelRatio();
  QImage img(
      std::ceil(logical_w * ratio), logical_h * ratio,
      QImage::Format_ARGB32_Premultiplied);
  img.setDevicePixelRatio(ratio);
  img.fill(Qt::transparent);
  return img;
}

std::optional<QPointF> mapPointToItem(QPoint global, QGraphicsItem& item)
{
  // Get the QGraphicsView
  auto views = item.scene()->views();
  if(views.empty())
    return std::nullopt;

  auto view = views.front();

  // Find where to paste in the scenario
  auto view_pt = view->mapFromGlobal(global);
  auto scene_pt = view->mapToScene(view_pt);
  return item.mapFromScene(scene_pt);
}

namespace score
{
std::pair<double, bool>
ItemBounder::bound(QGraphicsItem* parent, double x0, double w) noexcept
{
  auto view = getView(*parent);
  int item_left = view->mapFromScene(parent->mapToScene({x0, 0.})).x();
  int item_right = item_left + w;

  double x = x0;
  const double min_x = x0;
  const double max_x = view->width() - 30.;

  if(item_left <= min_x)
  {
    // Compute the pixels needed to add to have top-left at 0
    x = x - item_left + min_x;
  }
  else if(item_right >= max_x)
  {
    // Compute the pixels needed to add to have top-right at max
    x = x - item_right + max_x;
  }
  x = std::max(x, 2 * x0);

  if(std::abs(m_x - x) > 0.1)
  {
    m_x = x;
    return {x, true};
  }
  else
  {
    return {x, false};
  }
}
}
