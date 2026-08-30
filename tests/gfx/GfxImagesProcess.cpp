// Gfx/Images: the process model, its image-list control port, the shared
// ImageCache behind it, and the list-editing widget.
//
// Nothing in tests/ names Gfx::Images. The Images process is the only one whose
// port surface is driven by *files on disk*: the index spin box's domain is
// recomputed from the frame count of the loaded images, and every path goes
// through a process-global refcounted cache that two processes can share. That
// is where the interesting failure modes are — a missing file, a file whose
// bytes are not an image, an index that no longer has an image behind it, and
// two processes releasing the same cached entry.
//
// Model-level only: no rendering is asserted (ImagesNode is a render node and
// is not built here).

#include "GfxProcessDoc.hpp"

#include <Gfx/Images/ImageListChooser.hpp>
#include <Gfx/Images/Process.hpp>

#include <Process/Commands/SetControlValue.hpp>
#include <Process/Dataflow/WidgetInlets.hpp>

#include <score/document/DocumentContext.hpp>

#include <State/Domain.hpp>

#include <ossia/network/domain/domain.hpp>
#include <ossia/network/domain/domain_functions.hpp>
#include <ossia/network/value/value_conversion.hpp>

#include <QAbstractItemModel>
#include <QApplication>
#include <QImage>
#include <QListView>
#include <QPushButton>

#include <catch2/catch_test_macros.hpp>

using namespace score::test;
using namespace score::test::gfxproc;

namespace
{
constexpr auto UUID_IMAGES = "e96c5c0b-7e09-49fb-a851-ff6f4811bb00";

/// A real, decodable PNG on disk. PNG is built into QtGui, so this does not
/// depend on any image-format plugin being deployed next to the test.
QString write_png(const QString& dir, const char* name, QRgb color, QSize sz = {8, 8})
{
  QImage img{sz, QImage::Format_ARGB32};
  img.fill(color);
  const QString path = dir + "/" + QString::fromUtf8(name);
  REQUIRE(img.save(path, "PNG"));
  return path;
}

/// A file with an image extension whose bytes are not an image.
QString write_corrupt(const QString& dir, const char* name)
{
  const QString path = dir + "/" + QString::fromUtf8(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write("\x89PNG\r\n\x1a\n not actually a png at all, truncated garbage");
  f.close();
  return path;
}

ossia::value paths_value(const std::vector<QString>& paths)
{
  std::vector<ossia::value> v;
  for(auto& p : paths)
    v.push_back(p.toStdString());
  return v;
}

std::vector<std::string> value_paths(const ossia::value& v)
{
  std::vector<std::string> out;
  for(auto& e : ossia::convert<std::vector<ossia::value>>(v))
    out.push_back(ossia::convert<std::string>(e));
  return out;
}

Gfx::Images::ImageListChooser& list_port(Process::ProcessModel& m)
{
  for(auto* inl : m.inlets())
    if(auto* c = dynamic_cast<Gfx::Images::ImageListChooser*>(inl))
      return *c;
  FAIL("no ImageListChooser inlet");
  throw;
}

Process::IntSpinBox& index_port(Process::ProcessModel& m)
{
  for(auto* inl : m.inlets())
    if(auto* c = dynamic_cast<Process::IntSpinBox*>(inl))
      return *c;
  FAIL("no index inlet");
  throw;
}
}

TEST_CASE("The Images process exposes its documented control surface", "[gfx][images][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_IMAGES);
    REQUIRE(proc != nullptr);

    // Index, Opacity, Position, Scale X, Scale Y, Images, Tile, Scale.
    CHECK(proc->inlets().size() == 8);
    CHECK(proc->outlets().size() == 1);
    CHECK(inlet_names(*proc).front() == QStringLiteral("Index"));

    // With no images, the index domain is pinned to [0, 0] rather than left
    // unbounded: an out-of-range index can never be dialled in.
    auto& idx = index_port(*proc);
    const auto& dom = idx.domain().get();
    CHECK(ossia::convert<int>(ossia::get_min(dom)) == 0);
    CHECK(ossia::convert<int>(ossia::get_max(dom)) == 0);
  });
}

TEST_CASE("Loading an image list drives the index domain", "[gfx][images][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    const QString dir = scratch_dir("images-domain");
    const QString a = write_png(dir, "a.png", qRgb(255, 0, 0));
    const QString b = write_png(dir, "b.png", qRgb(0, 255, 0));

    auto* proc = add_process(ctx, *doc, UUID_IMAGES);
    REQUIRE(proc != nullptr);
    auto& images = list_port(*proc);
    auto& idx = index_port(*proc);

    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::SetControlValue>(images, paths_value({a, b}));

    CHECK(value_paths(images.value()).size() == 2);
    // Two single-frame images -> frames 0 and 1.
    CHECK(ossia::convert<int>(ossia::get_max(idx.domain().get())) == 1);

    // Undo must take both the list AND the derived domain back.
    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(value_paths(images.value()).empty());
    CHECK(ossia::convert<int>(ossia::get_max(idx.domain().get())) == 0);

    REQUIRE(stack.canRedo());
    stack.redo();
    CHECK(value_paths(images.value()).size() == 2);
    CHECK(ossia::convert<int>(ossia::get_max(idx.domain().get())) == 1);
  });
}

TEST_CASE("Unreadable and corrupt images are dropped, not loaded", "[gfx][images][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    const QString dir = scratch_dir("images-bad");
    const QString good = write_png(dir, "good.png", qRgb(0, 0, 255));
    const QString corrupt = write_corrupt(dir, "corrupt.png");
    const QString missing = dir + "/does-not-exist.png";
    const QString unsupported = dir + "/notanimage.xyz";
    {
      QFile f{unsupported};
      REQUIRE(f.open(QIODevice::WriteOnly));
      f.write("hello");
    }

    auto* proc = add_process(ctx, *doc, UUID_IMAGES);
    REQUIRE(proc != nullptr);
    auto& images = list_port(*proc);
    auto& idx = index_port(*proc);

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::SetControlValue>(
        images, paths_value({good, corrupt, missing, unsupported}));

    // The control keeps every path the user typed...
    CHECK(value_paths(images.value()).size() == 4);
    // ...but only the one decodable image contributes a frame, so the index
    // domain must not offer indices with nothing behind them.
    CHECK(ossia::convert<int>(ossia::get_max(idx.domain().get())) == 0);

    // getImages() is the same call the executor makes: it must skip the three
    // bad paths silently rather than throw or produce empty Image entries.
    auto imgs = Gfx::getImages(images.value(), doc->context());
    CHECK(imgs.size() == 1);
    if(!imgs.empty())
    {
      CHECK(imgs[0].path == good);
      CHECK(imgs[0].frames.size() == 1);
    }
    Gfx::releaseImages(imgs);
  });
}

TEST_CASE("fromImageSet round-trips the image paths", "[gfx][images][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    const QString dir = scratch_dir("images-roundtrip");
    const QString a = write_png(dir, "r0.png", qRgb(10, 20, 30));
    const QString b = write_png(dir, "r1.png", qRgb(40, 50, 60));

    auto in = paths_value({a, b});
    auto imgs = Gfx::getImages(in, doc->context());
    REQUIRE(imgs.size() == 2);

    const auto back = Gfx::fromImageSet(std::span<score::gfx::Image>{imgs});
    CHECK(value_paths(back) == value_paths(in));

    Gfx::releaseImages(imgs);
    CHECK(imgs.empty());
  });
}

// FINDING (defect, filed by this test): Gfx::ImageCache::acquire() inserts a
// NEW entry with refcount 0 rather than 1 (Gfx/Images/Process.cpp,
// `m_images.insert({path, {0, *std::move(img)}})`), while release() does
// `--count; if(count <= 0) erase`. So the FIRST release always evicts, no
// matter how many holders there are: two Images processes listing the same
// file, one of them removed, and the entry is gone while the other still uses
// it. Nothing crashes — score::gfx::Image copies its QImage frames — but the
// cache stops being a cache exactly in the case it exists for, and the next
// acquire re-decodes from disk.
//
// Expected-failure so that fixing the refcount turns this case green and
// Catch2 flags the [!shouldfail].
TEST_CASE(
    "The image cache survives two processes sharing one file",
    "[gfx][images][gui][!shouldfail]")
{
  // The documented scenario: two Images processes list the same file. The
  // cache is a process-global refcount, so whichever process releases first
  // must NOT invalidate the other one's entry.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    const QString dir = scratch_dir("images-cache");
    const QString shared = write_png(dir, "shared.png", qRgb(1, 2, 3), QSize{4, 4});

    auto& cache = Gfx::ImageCache::instance();

    auto first = cache.acquire(shared);
    REQUIRE(first.has_value());
    auto second = cache.acquire(shared);
    REQUIRE(second.has_value());
    CHECK(second->path == shared);
    CHECK(second->frames.size() == 1);

    // One holder goes away. The other is still using the entry.
    cache.release(std::move(*first));

    // Rewrite the file with different content. If the entry is still cached,
    // a third acquire must return the ORIGINAL 4x4 decode (that is what a
    // cache is for, and what the second holder is still pointing at). If the
    // release above evicted it, this re-reads from disk and comes back 16x16.
    write_png(dir, "shared.png", qRgb(9, 9, 9), QSize{16, 16});

    auto third = cache.acquire(shared);
    REQUIRE(third.has_value());
    REQUIRE(third->frames.size() == 1);
    CHECK(third->frames[0].size() == QSize{4, 4});

    cache.release(std::move(*second));
    cache.release(std::move(*third));
  });
}

TEST_CASE("An unreadable path is not cached", "[gfx][images][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext&) {
    auto& cache = Gfx::ImageCache::instance();
    CHECK_FALSE(cache.acquire(QStringLiteral("/nonexistent/nope.png")).has_value());
    CHECK_FALSE(cache.acquire(QStringLiteral("")).has_value());
  });
}

TEST_CASE("The image-list editor writes back through an undoable command", "[gfx][images][gui]")
{
  // WidgetFactory::ImageListChooserItems::make_widget builds the EditableTable
  // list editor and wires its "-" button to a SetControlValue command. That is
  // the only path by which a user removes an image, and it is the only user of
  // EditableTable at all.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    const QString dir = scratch_dir("images-editor");
    const QString a = write_png(dir, "e0.png", qRgb(1, 1, 1));
    const QString b = write_png(dir, "e1.png", qRgb(2, 2, 2));
    const QString c = write_png(dir, "e2.png", qRgb(3, 3, 3));

    auto* proc = add_process(ctx, *doc, UUID_IMAGES);
    REQUIRE(proc != nullptr);
    auto& images = list_port(*proc);

    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Process::SetControlValue>(images, paths_value({a, b, c}));

    QObject context;
    QWidget* w = WidgetFactory::ImageListChooserItems::make_widget(
        images, doc->context(), nullptr, &context);
    REQUIRE(w != nullptr);

    auto* view = w->findChild<QListView*>();
    REQUIRE(view != nullptr);
    REQUIRE(view->model() != nullptr);
    CHECK(view->model()->rowCount() == 3);

    // The "-" button removes the selected row and submits the new list.
    QPushButton* minus{};
    for(auto* btn : w->findChildren<QPushButton*>())
      if(btn->text() == QStringLiteral("-"))
        minus = btn;
    REQUIRE(minus != nullptr);

    view->setCurrentIndex(view->model()->index(1, 0));
    minus->click();
    QApplication::processEvents();

    CHECK(view->model()->rowCount() == 2);
    CHECK(value_paths(images.value()).size() == 2);

    REQUIRE(stack.canUndo());
    stack.undo();
    QApplication::processEvents();
    CHECK(value_paths(images.value()).size() == 3);
    // The editor listens on the port, so undo must be reflected in the widget.
    CHECK(view->model()->rowCount() == 3);

    delete w;
  });
}
