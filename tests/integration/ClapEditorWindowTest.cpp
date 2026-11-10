// The window score puts a CLAP plug-in's editor in follows what the plug-in
// says of its size (clap-sized-editor): a window manager may resize the
// window of an editor that can be resized, and the plug-in is told the new
// size; the window of a fixed-size editor stays at that size.
//
// Sizes are in the editor's own pixels (clap/ext/gui.h): physical pixels on
// X11 and Win32, points on Cocoa. Each case runs as is and with
// QT_SCALE_FACTOR=2, where a logical pixel of Qt is two of the editor's on
// every platform.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Effect/EffectLayer.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QWidget>

#include <score/widgets/PluginWindow.hpp>

#include <Clap/EffectModel.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <dlfcn.h>

#include <thread>

namespace
{
const QString clap_uuid = QStringLiteral("10e8d26d-1d82-4bfc-a9fb-d85ffdf04e5f");

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QApplication::processEvents();
}

//! Runs the application with Qt scaling its logical pixels by r: editor
//! pixels per logical pixel, as Xvfb and a screen of ratio 1 have no scaling
//! of their own. On Cocoa a Retina backing scale would not be part of it.
struct qt_scale
{
  explicit qt_scale(int r)
  {
    if(r != 1)
      qputenv("QT_SCALE_FACTOR", QByteArray::number(r));
  }
  ~qt_scale() { qunsetenv("QT_SCALE_FACTOR"); }
};

//! The scale the host last gave the editor, 0 for none
double plugin_scale(const Clap::Model& proc)
{
  void* lib = dlopen(CLAP_SIZED_EDITOR_PATH, RTLD_LAZY | RTLD_NOLOAD);
  REQUIRE(lib);
  auto f = reinterpret_cast<double (*)(const clap_plugin_t*)>(
      dlsym(lib, "clap_sized_editor_scale"));
  REQUIRE(f);
  return f(proc.handle()->plugin);
}

//! The size the host last gave the editor, and how many times it gave one
int plugin_size(const Clap::Model& proc, uint32_t& w, uint32_t& h)
{
  void* lib = dlopen(CLAP_SIZED_EDITOR_PATH, RTLD_LAZY | RTLD_NOLOAD);
  REQUIRE(lib);
  auto f = reinterpret_cast<int (*)(const clap_plugin_t*, uint32_t*, uint32_t*)>(
      dlsym(lib, "clap_sized_editor_size"));
  REQUIRE(f);
  return f(proc.handle()->plugin, &w, &h);
}

//! How many calls of its gui extension the editor got off the main thread
int plugin_off_main_thread(const Clap::Model& proc)
{
  void* lib = dlopen(CLAP_SIZED_EDITOR_PATH, RTLD_LAZY | RTLD_NOLOAD);
  REQUIRE(lib);
  auto f = reinterpret_cast<int (*)(const clap_plugin_t*)>(
      dlsym(lib, "clap_sized_editor_off_main_thread"));
  REQUIRE(f);
  return f(proc.handle()->plugin);
}

//! Calls f on a thread of its own, as an editor may from its own threads
template <typename F>
auto from_another_thread(F f)
{
  decltype(f()) res{};
  std::thread t{[&] { res = f(); }};
  t.join();
  return res;
}

Clap::Model& add_editor(score::Document& doc, const char* id)
{
  auto* proc = score::test::add_process(
      doc, clap_uuid, QStringLiteral(CLAP_SIZED_EDITOR_PATH ":::") + id);
  REQUIRE(proc);
  auto& clap = static_cast<Clap::Model&>(*proc);
  REQUIRE(clap.handle());
  REQUIRE(clap.handle()->plugin);
  return clap;
}
}

TEST_CASE(
    "CLAP: the window of an editor that can be resized can be resized",
    "[integration][clap][gui]")
{
  const int r = GENERATE(1, 2);
  INFO("QT_SCALE_FACTOR " << r);
  qt_scale scale{r};
  score::test::run_in_gui_app([r](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = add_editor(*doc, "io.ossia.test.resizable-editor");

    Process::setupExternalUI(proc, doc->context(), true);
    spin(200);
    QWidget* win = proc.externalUI;
    REQUIRE(win);
    CHECK(win->size() == QSize(320 / r, 200 / r));
    // What a window manager reads to let the user resize it
    CHECK(win->minimumSize() != win->maximumSize());

    win->resize(200, 150);
    spin(200);
    uint32_t w{}, h{};
    CHECK(plugin_size(proc, w, h) > 0);
    CHECK(w == 200 * r);
    CHECK(h == 150 * r);

    Process::setupExternalUI(proc, doc->context(), false);
    spin(100);
  });
}

TEST_CASE(
    "CLAP: the window of a resizable editor takes the size the editor chose",
    "[integration][clap][gui]")
{
  const int r = GENERATE(1, 2);
  INFO("QT_SCALE_FACTOR " << r);
  qt_scale scale{r};
  score::test::run_in_gui_app([r](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = add_editor(*doc, "io.ossia.test.resizable-editor");

    Process::setupExternalUI(proc, doc->context(), true);
    spin(200);
    QWidget* win = proc.externalUI;
    REQUIRE(win);

    // Narrower than the 100 x 100 the editor takes at least
    win->resize(30, 120);
    spin(200);
    uint32_t w{}, h{};
    CHECK(plugin_size(proc, w, h) > 0);
    CHECK(w == 100);
    CHECK(h == 120 * r);
    CHECK(win->size() == QSize(100 / r, 120));

    Process::setupExternalUI(proc, doc->context(), false);
    spin(100);
  });
}

TEST_CASE("CLAP: the window of a fixed-size editor keeps its size", "[integration][clap][gui]")
{
  const int r = GENERATE(1, 2);
  INFO("QT_SCALE_FACTOR " << r);
  qt_scale scale{r};
  score::test::run_in_gui_app([r](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = add_editor(*doc, "io.ossia.test.fixed-editor");

    Process::setupExternalUI(proc, doc->context(), true);
    spin(200);
    QWidget* win = proc.externalUI;
    REQUIRE(win);
    CHECK(win->size() == QSize(320 / r, 200 / r));
    CHECK(win->minimumSize() == QSize(320 / r, 200 / r));
    CHECK(win->maximumSize() == QSize(320 / r, 200 / r));
#if defined(__APPLE__)
    // clap/ext/gui.h: Cocoa "uses logical size, don't call set_scale()"
    CHECK(plugin_scale(proc) == 0.);
#else
    CHECK(plugin_scale(proc) == double(r));
#endif

    Process::setupExternalUI(proc, doc->context(), false);
    spin(100);
  });
}

TEST_CASE("Editor pixels per logical pixel of Qt", "[integration][clap][vst][vst3]")
{
  // A Retina screen without scaling of Qt's own, a screen of ratio 1 with it
#if defined(__APPLE__)
  STATIC_CHECK(score::PluginWindow::editorPixelRatio(2., 1.) == 1.);
  STATIC_CHECK(score::PluginWindow::editorPixelRatio(4., 2.) == 2.);
#else
  STATIC_CHECK(score::PluginWindow::editorPixelRatio(2., 1.) == 2.);
  STATIC_CHECK(score::PluginWindow::editorPixelRatio(2., 2.) == 2.);
#endif
}

TEST_CASE(
    "CLAP: an editor's requests from another thread are carried out on the main one",
    "[integration][clap][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = add_editor(*doc, "io.ossia.test.resizable-editor");

    Process::setupExternalUI(proc, doc->context(), true);
    spin(200);
    QWidget* win = proc.externalUI;
    REQUIRE(win);
    REQUIRE(win->isVisible());

    const clap_host_t* host = &proc.handle()->host;
    auto* gui = static_cast<const clap_host_gui_t*>(host->get_extension(host, CLAP_EXT_GUI));
    REQUIRE(gui);

    // clap/ext/gui.h: off the main thread, true means the host took the
    // request and carries it out later
    CHECK(from_another_thread([&] { return gui->request_resize(host, 240, 180); }));
    spin(200);
    CHECK(win->size() == QSize(240, 180));
    uint32_t w{}, h{};
    CHECK(plugin_size(proc, w, h) > 0);
    CHECK(w == 240);
    CHECK(h == 180);

    from_another_thread([&] {
      gui->resize_hints_changed(host);
      return true;
    });
    spin(100);
    CHECK(win->minimumSize() != win->maximumSize());

    CHECK(from_another_thread([&] { return gui->request_hide(host); }));
    spin(200);
    CHECK(!win->isVisible());
    CHECK(from_another_thread([&] { return gui->request_show(host); }));
    spin(200);
    CHECK(win->isVisible());

    CHECK(plugin_off_main_thread(proc) == 0);

    Process::setupExternalUI(proc, doc->context(), false);
    spin(100);
  });
}
