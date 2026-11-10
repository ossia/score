// The window score puts a CLAP plug-in's editor in follows what the plug-in
// says of its size (clap-sized-editor): a window manager may resize the
// window of an editor that can be resized, and the plug-in is told the new
// size; the window of a fixed-size editor stays at that size.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Project.hpp>

#include <Effect/EffectLayer.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <QElapsedTimer>
#include <QWidget>

#include <Clap/EffectModel.hpp>
#include <catch2/catch_test_macros.hpp>

#include <dlfcn.h>

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
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = add_editor(*doc, "io.ossia.test.resizable-editor");

    Process::setupExternalUI(proc, doc->context(), true);
    spin(200);
    QWidget* win = proc.externalUI;
    REQUIRE(win);
    CHECK(win->size() == QSize(320, 200));
    // What a window manager reads to let the user resize it
    CHECK(win->minimumSize() != win->maximumSize());

    win->resize(400, 300);
    spin(200);
    uint32_t w{}, h{};
    CHECK(plugin_size(proc, w, h) > 0);
    CHECK(w == 400);
    CHECK(h == 300);

    Process::setupExternalUI(proc, doc->context(), false);
    spin(100);
  });
}

TEST_CASE(
    "CLAP: the window of a resizable editor takes the size the editor chose",
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

    // Smaller than the 100 x 100 the editor takes at least
    win->resize(60, 250);
    spin(200);
    uint32_t w{}, h{};
    CHECK(plugin_size(proc, w, h) > 0);
    CHECK(w == 100);
    CHECK(h == 250);
    CHECK(win->size() == QSize(100, 250));

    Process::setupExternalUI(proc, doc->context(), false);
    spin(100);
  });
}

TEST_CASE("CLAP: the window of a fixed-size editor keeps its size", "[integration][clap][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& proc = add_editor(*doc, "io.ossia.test.fixed-editor");

    Process::setupExternalUI(proc, doc->context(), true);
    spin(200);
    QWidget* win = proc.externalUI;
    REQUIRE(win);
    CHECK(win->size() == QSize(320, 200));
    CHECK(win->minimumSize() == QSize(320, 200));
    CHECK(win->maximumSize() == QSize(320, 200));

    Process::setupExternalUI(proc, doc->context(), false);
    spin(100);
  });
}
