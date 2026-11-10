// A VST3 editor and its window keep one size: the window the user resizes
// takes the size the editor accepts, the editor is told it once, and the
// window takes the sizes the editor asks for, at any scaling of Qt
// (QT_SCALE_FACTOR, set per ctest entry). Made with vst3-fixtures/ResizableView,
// an editor that only takes some sizes and logs what the host tells it.
//
// On X11 through xdotool, as a window manager resizes a window: run on a
// private X server (tests/tools/run-with-display.sh).

#include <Process/Process.hpp>

#include <Effect/EffectLayer.hpp>

#include <Vst3/ApplicationPlugin.hpp>
#include <Vst3/EffectModel.hpp>

#include <score/widgets/PluginWindow.hpp>

#include <core/document/Document.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QWidget>
#include <QtGui/qguiapplication_platform.h>

#include <catch2/catch_test_macros.hpp>
#include <xcb/xcb.h>

#include <dlfcn.h>

#include <cmath>

namespace
{
const QString vst3_uuid = QStringLiteral("4cc30435-3237-4e94-a79e-1c2bd6b724a1");

enum mode
{
  plain = 0,
  confirms = 1,
  grows = 2,
  fixed = 3
};

QString processor_uid(vst3::ApplicationPlugin& plug)
{
  auto mdl = plug.getModule(SCORE_VST3_RESIZABLE_VIEW);
  REQUIRE(mdl);
  vst3::AvailablePlugin p;
  p.path = QStringLiteral(SCORE_VST3_RESIZABLE_VIEW);
  p.name = QString::fromStdString(mdl->getName());
  QString uid;
  for(const auto& cls : mdl->getFactory().classInfos())
    if(cls.category() == kVstAudioEffectClass)
    {
      p.classInfo.push_back(cls);
      uid = QString::fromStdString(cls.ID().toString());
    }
  p.isValid = true;
  plug.vst_infos = {p};
  return uid;
}

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
}

template <typename F>
F fixture(const char* name)
{
  void* lib = dlopen(
      SCORE_VST3_RESIZABLE_VIEW "/Contents/x86_64-linux/score-test-resizable-view.so",
      RTLD_LAZY | RTLD_NOLOAD);
  REQUIRE(lib);
  auto f = reinterpret_cast<F>(dlsym(lib, name));
  REQUIRE(f);
  return f;
}

QStringList take_log()
{
  const QString log = QString::fromUtf8(
      fixture<const char* (*)()>("score_test_resizable_view_log")());
  fixture<void (*)()>("score_test_resizable_view_clear")();
  return log.split('\n', Qt::SkipEmptyParts);
}

int count(const QStringList& log, const QString& prefix)
{
  int n = 0;
  for(const auto& l : log)
    n += l.startsWith(prefix);
  return n;
}

//! The sizes of the onSize() calls in the log
QList<QSize> on_sizes(const QStringList& log)
{
  static const QRegularExpression re{QStringLiteral("^onSize (-?\\d+)x(-?\\d+)$")};
  QList<QSize> r;
  for(const auto& l : log)
    if(auto m = re.match(l); m.hasMatch())
      r.push_back(QSize(m.captured(1).toInt(), m.captured(2).toInt()));
  return r;
}

xcb_connection_t* xconn()
{
  if(auto x11 = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
    return x11->connection();
  return nullptr;
}

QSize x_size(xcb_window_t w)
{
  QSize s;
  if(auto gr = xcb_get_geometry_reply(xconn(), xcb_get_geometry(xconn(), w), nullptr))
  {
    s = QSize(gr->width, gr->height);
    free(gr);
  }
  return s;
}

//! The window of the editor, inside the window of score
QSize x_child_size(xcb_window_t w)
{
  QSize s;
  auto c = xconn();
  auto tr = xcb_query_tree_reply(c, xcb_query_tree(c, w), nullptr);
  if(!tr)
    return s;
  auto* kids = xcb_query_tree_children(tr);
  for(int i = 0, n = xcb_query_tree_children_length(tr); i < n; i++)
  {
    auto ar = xcb_get_window_attributes_reply(c, xcb_get_window_attributes(c, kids[i]), nullptr);
    const bool viewable = ar && ar->map_state == XCB_MAP_STATE_VIEWABLE;
    free(ar);
    if(!viewable)
      continue;
    const QSize k = x_size(kids[i]);
    if(k.width() * k.height() > s.width() * s.height())
      s = k;
  }
  free(tr);
  return s;
}

bool xdo(const QStringList& args)
{
  QProcess p;
  p.start(QStringLiteral("xdotool"), args);
  if(!p.waitForStarted(2000))
    return false;
  QElapsedTimer t;
  t.start();
  while(p.state() != QProcess::NotRunning && t.elapsed() < 5000)
  {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    p.waitForFinished(5);
  }
  return p.state() == QProcess::NotRunning && p.exitCode() == 0;
}

QSize plugin_size(vst3::Model& m)
{
  Steinberg::ViewRect r;
  if(m.fx.view && m.fx.view->getSize(&r) == Steinberg::kResultOk)
    return QSize(r.getWidth(), r.getHeight());
  return {};
}

bool takes(QSize s)
{
  return s.width() >= 300 && s.width() % 30 == 0 && s.height() == s.width() * 2 / 3;
}

double scale_factor()
{
  const double f = qEnvironmentVariable("QT_SCALE_FACTOR").toDouble();
  return f > 0. ? f : 1.;
}

//! Native sizes from logical ones round at fractional scales
int slack()
{
  const double f = scale_factor();
  return f == std::floor(f) ? 0 : 1;
}

bool close_to(QSize a, QSize b)
{
  return std::abs(a.width() - b.width()) <= slack()
         && std::abs(a.height() - b.height()) <= slack();
}

std::string str(QSize s)
{
  return std::to_string(s.width()) + "x" + std::to_string(s.height());
}

std::string str(const QStringList& log)
{
  return log.join(QStringLiteral(" | ")).toStdString();
}

struct editor
{
  vst3::Model& model;
  QWidget* win{};
  xcb_window_t wid{};
  QStringList open_log;

  QSize x() const { return x_size(wid); }
  QSize child() const { return x_child_size(wid); }
  QSize plugin() const { return plugin_size(model); }

  //! The window, the editor's own window and what the editor says agree
  void agree() const
  {
    INFO("x " << str(x()) << " child " << str(child()) << " getSize " << str(plugin()));
    CHECK(close_to(x(), child()));
    CHECK(close_to(x(), plugin()));
    CHECK(child() == plugin());
  }
};

editor open(score::Document& doc, const QString& uid, int m)
{
  fixture<void (*)(int)>("score_test_resizable_view_mode")(m);
  take_log();
  auto* proc = score::test::add_process(doc, vst3_uuid, uid);
  REQUIRE(proc);
  // vst3::Model is not exported: no qobject_cast
  editor e{static_cast<vst3::Model&>(*proc)};
  Process::setupExternalUI(*proc, doc.context(), true);
  spin(1000);
  REQUIRE(proc->externalUI);
  e.win = proc->externalUI;
  e.wid = xcb_window_t(e.win->winId());
  e.open_log = take_log();
  return e;
}

void close(editor& e, score::Document& doc)
{
  Process::setupExternalUI(e.model, doc.context(), false);
  spin(300);
}

//! The window resized from outside, as a window manager does
QStringList resize_from_outside(editor& e)
{
  const QSize before = e.x();
  take_log();
  REQUIRE(xdo(
      {"windowsize", QString::number(e.wid), QString::number(before.width() + 120),
       QString::number(before.height() + 90)}));
  spin(1000);
  return take_log();
}

void opened_well(const editor& e)
{
  INFO("open: " << str(e.open_log));
  // The editor knows of no size before it has a window
  const auto first_on = std::find_if(e.open_log.begin(), e.open_log.end(), [](auto& l) {
    return l.startsWith("onSize");
  });
  const auto attached = std::find_if(e.open_log.begin(), e.open_log.end(), [](auto& l) {
    return l.startsWith("attached");
  });
  REQUIRE(attached != e.open_log.end());
  CHECK(first_on > attached);
  for(auto s : on_sizes(e.open_log))
    CHECK((s.width() > 0 && s.height() > 0));
}
}

TEST_CASE("VST3: an editor and its window keep one size", "[vst3][ui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    if(QGuiApplication::platformName() != QLatin1String("xcb")
       || QStandardPaths::findExecutable(QStringLiteral("xdotool")).isEmpty())
      SKIP("needs X11 and xdotool");

    auto& plug = ctx.applicationPlugin<vst3::ApplicationPlugin>();
    const QString uid = processor_uid(plug);
    REQUIRE(!uid.isEmpty());
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    INFO("QT_SCALE_FACTOR " << scale_factor());

    {
      INFO("an editor that takes some sizes");
      editor e = open(*doc, uid, plain);
      opened_well(e);
      e.agree();
      CHECK(takes(e.plugin()));

      const auto log = resize_from_outside(e);
      INFO("resized: " << str(log));
      // Asked once, told once
      CHECK(count(log, "check") == 1);
      CHECK(count(log, "onSize") == 1);
      e.agree();
      CHECK(takes(e.plugin()));
      CHECK(e.plugin().width() > 300 * scale_factor());

      // The window follows the editor, to the last size it asked for
      const QSize a = e.plugin() + QSize(30, 20), b = e.plugin() + QSize(60, 40);
      take_log();
      REQUIRE(fixture<int (*)(int, int, int, int)>("score_test_resizable_view_resize_twice")(
          a.width(), a.height(), b.width(), b.height()));
      spin(1000);
      const auto twice = take_log();
      INFO("asked twice: " << str(twice));
      CHECK(e.plugin() == b);
      CHECK(on_sizes(twice) == QList<QSize>{a, b});
      e.agree();
      close(e, *doc);
    }

    {
      INFO("an editor that answers onSize with the same size");
      editor e = open(*doc, uid, confirms);
      opened_well(e);
      CHECK(count(e.open_log, "onSize") <= 3);
      const auto log = resize_from_outside(e);
      INFO("resized: " << str(log));
      CHECK(count(log, "check") == 1);
      CHECK(count(log, "onSize") == 1);
      e.agree();
      close(e, *doc);
    }

    {
      INFO("an editor that answers onSize with a larger size");
      editor e = open(*doc, uid, grows);
      opened_well(e);
      CHECK(count(e.open_log, "onSize") <= 4);
      const auto log = resize_from_outside(e);
      INFO("resized: " << str(log));
      CHECK(count(log, "check") == 1);
      CHECK(count(log, "onSize") <= 2);
      e.agree();
      close(e, *doc);
    }

    {
      INFO("an editor that cannot be resized");
      editor e = open(*doc, uid, fixed);
      opened_well(e);
      e.agree();
      const double f = score::PluginWindow::editorPixelRatio(*e.win);
      const QSize qt_size{
          int(std::lround(e.plugin().width() / f)), int(std::lround(e.plugin().height() / f))};
      CHECK(e.win->minimumSize() == qt_size);
      CHECK(e.win->maximumSize() == qt_size);
      const auto log = resize_from_outside(e);
      INFO("resized: " << str(log));
      CHECK(count(log, "check") == 0);
      CHECK(count(log, "onSize") == 0);
      CHECK(e.win->minimumSize() == qt_size);
      CHECK(e.win->maximumSize() == qt_size);
      close(e, *doc);
    }
  });
}
