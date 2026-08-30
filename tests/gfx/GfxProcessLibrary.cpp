// The library and drop layer around the Gfx processes: which file lands on
// which process when you drag it into the score, and what the Shadertoy import
// does with a URL.
//
// Everything here goes through the real registries — ProcessDropHandlerList
// and LibraryInterfaceList, looked up by the SCORE_CONCRETE uuid — because
// that is what the application does, and because the handler classes are not
// exported from the shared plug-in (and their overrides are private, so they
// could only ever be called through the base anyway).
//
// NETWORK: Gfx::Filter::DropHandler::dropCustom downloads from shadertoy.com
// for any URL whose host ends in "shadertoy.com" AND whose path starts with
// "/view/". This test deliberately drops URLs that satisfy the FIRST condition
// and not the second, so the two guards are exercised and no request is ever
// made. Do not add a real /view/<id> URL here.

#include "GfxProcessDoc.hpp"

#include <Process/Drop/ProcessDropHandler.hpp>

#include <Library/LibraryInterface.hpp>

#include <QImage>
#include <QMimeData>
#include <QUrl>

#include <catch2/catch_test_macros.hpp>

using namespace score::test;
using namespace score::test::gfxproc;

namespace
{
// Process factories
constexpr auto UUID_P_FILTER = "74ca45ff-92c9-44a0-8f1a-754dea05ee1b";
constexpr auto UUID_P_CSF = "a5bbffe0-93d2-4e70-995c-cf46c2c43520";
constexpr auto UUID_P_IMAGES = "e96c5c0b-7e09-49fb-a851-ff6f4811bb00";
constexpr auto UUID_P_VIDEO = "32dc5341-7748-4c31-a226-82e6bd685744";

// Drop handlers
constexpr auto UUID_D_FILTER = "d1e16bba-4c53-4d24-8b6b-71b94daef68d";
constexpr auto UUID_D_FILTER_TEX = "e9bf6cf8-c872-4638-b98a-ed76edc8e2dd";
constexpr auto UUID_D_CSF = "b3adba36-29cc-45b4-bea3-5a2a89458a48";
constexpr auto UUID_D_IMAGES = "f37aa176-d8be-45bc-b833-d014efba6157";
constexpr auto UUID_D_VIDEO = "12d1ed39-0fac-43da-8520-b7e32f9fad7d";

// Library handlers
constexpr auto UUID_L_FILTER = "e62ed6f6-a2c1-4d27-a9c3-1c3bc576bfeb";
constexpr auto UUID_L_CSF = "b5c5800f-2e84-4e29-9c7c-39577e6e6fa0";
constexpr auto UUID_L_IMAGES = "0916759f-a5f6-4870-a96b-4e1e5efe5885";
constexpr auto UUID_L_VIDEO = "be66d573-571f-4c33-9f60-0791f53c7266";

Process::ProcessDropHandler*
dropper(const score::GUIApplicationContext& ctx, const char* uuid)
{
  return ctx.interfaces<Process::ProcessDropHandlerList>().get(
      UuidKey<Process::ProcessDropHandler>::fromString(QString::fromUtf8(uuid)));
}

Library::LibraryInterface*
librarian(const score::GUIApplicationContext& ctx, const char* uuid)
{
  return ctx.interfaces<Library::LibraryInterfaceList>().get(
      UuidKey<Library::LibraryInterface>::fromString(QString::fromUtf8(uuid)));
}

UuidKey<Process::ProcessModel> pkey(const char* uuid)
{
  return UuidKey<Process::ProcessModel>::fromString(QString::fromUtf8(uuid));
}

std::vector<Process::ProcessDropHandler::ProcessDrop>
drop_urls(const score::GUIApplicationContext& ctx, score::Document& doc,
          const QList<QUrl>& urls)
{
  QMimeData mime;
  mime.setUrls(urls);
  return ctx.interfaces<Process::ProcessDropHandlerList>().getDrop(mime, doc.context());
}

bool has_key(
    const std::vector<Process::ProcessDropHandler::ProcessDrop>& drops,
    const UuidKey<Process::ProcessModel>& k)
{
  for(auto& d : drops)
    if(d.creation.key == k)
      return true;
  return false;
}

/// A file of `name` in the test's scratch dir with the given bytes.
QString file_with(const char* name, const QByteArray& bytes)
{
  const QString p = scratch_dir("library") + "/" + QString::fromUtf8(name);
  QFile f{p};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(bytes);
  f.close();
  return p;
}
}

TEST_CASE("Every Gfx drop and library handler is registered", "[gfx][library][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    for(auto* u : {UUID_D_FILTER, UUID_D_FILTER_TEX, UUID_D_IMAGES, UUID_D_VIDEO})
    {
      INFO(u);
      CHECK(dropper(ctx, u) != nullptr);
    }
    for(auto* u : {UUID_L_FILTER, UUID_L_CSF, UUID_L_IMAGES, UUID_L_VIDEO})
    {
      INFO(u);
      CHECK(librarian(ctx, u) != nullptr);
    }
  });
}

// FINDING (defect, filed by this test): Gfx::CSF::DropHandler is declared in
// Gfx/CSF/Library.hpp with uuid b3adba36-…, fully implements mimeTypes /
// fileExtensions / dropPath / dropCustom, and is never registered:
// score_plugin_gfx.cpp's FW<Process::ProcessDropHandler, …> list (the
// registration around line 173) names Filter, Video, Images and
// VideoTextureDropHandler, and stops there. Gfx::CSF::LibraryHandler IS in the
// FW<Library::LibraryInterface, …> list right below it, so a .cs compute shader
// appears in the library tree and cannot be dragged into a score at all.
//
// Fixed: CSF and VSA dropped the two overrides they declared and never defined,
// which is what left their vtables undefined, and all three handlers are
// registered.
TEST_CASE(
    "A compute shader can be dropped into the score",
    "[gfx][library][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    REQUIRE(dropper(ctx, UUID_D_CSF) != nullptr);
    const auto drops
        = drop_urls(ctx, *doc, {QUrl::fromLocalFile(corpus("csf-gradient-y.cs"))});
    REQUIRE_FALSE(drops.empty());
    CHECK(has_key(drops, pkey(UUID_P_CSF)));
  });
}

TEST_CASE("The Gfx file extensions do not overlap", "[gfx][library][gui]")
{
  // ProcessDropHandlerList keeps ONE handler per extension: two handlers
  // claiming the same suffix means whichever registers last silently wins and
  // the other process becomes undroppable.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* filter = dropper(ctx, UUID_D_FILTER);
    auto* images = dropper(ctx, UUID_D_IMAGES);
    auto* video = dropper(ctx, UUID_D_VIDEO);
    REQUIRE(filter);
    REQUIRE(images);
    REQUIRE(video);

    const auto fe = filter->fileExtensions();
    const auto ie = images->fileExtensions();
    const auto ve = video->fileExtensions();

    CHECK(fe.contains(QStringLiteral("fs")));
    CHECK(ie.contains(QStringLiteral("png")));
    CHECK(ve.contains(QStringLiteral("mp4")));

    CHECK((fe & ie).isEmpty());
    CHECK((fe & ve).isEmpty());
    CHECK((ie & ve).isEmpty());

    // The library scanners must accept what the drop handlers accept, else a
    // file can be dropped but never appears in the library tree.
    CHECK(librarian(ctx, UUID_L_FILTER)->acceptedFiles().contains(QStringLiteral("fs")));
    CHECK(librarian(ctx, UUID_L_IMAGES)->acceptedFiles() == ie);
    CHECK(librarian(ctx, UUID_L_VIDEO)->acceptedFiles().contains(QStringLiteral("mp4")));
  });
}

TEST_CASE("Dropping a file picks the right Gfx process", "[gfx][library][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    SECTION("an ISF fragment shader becomes an ISF filter")
    {
      const auto drops
          = drop_urls(ctx, *doc, {QUrl::fromLocalFile(corpus("isf-solid-color.fs"))});
      REQUIRE_FALSE(drops.empty());
      CHECK(has_key(drops, pkey(UUID_P_FILTER)));
    }

    SECTION("a PNG becomes an Images process")
    {
      QImage img{4, 4, QImage::Format_ARGB32};
      img.fill(qRgb(1, 2, 3));
      const QString p = scratch_dir("library") + "/drop.png";
      REQUIRE(img.save(p, "PNG"));

      const auto drops = drop_urls(ctx, *doc, {QUrl::fromLocalFile(p)});
      REQUIRE_FALSE(drops.empty());
      CHECK(has_key(drops, pkey(UUID_P_IMAGES)));
    }

    SECTION("a file that exists but has no Gfx handler yields nothing gfx-y")
    {
      const QString p = file_with("plain.unknownext", "nothing");
      const auto drops = drop_urls(ctx, *doc, {QUrl::fromLocalFile(p)});
      CHECK_FALSE(has_key(drops, pkey(UUID_P_FILTER)));
      CHECK_FALSE(has_key(drops, pkey(UUID_P_IMAGES)));
      CHECK_FALSE(has_key(drops, pkey(UUID_P_VIDEO)));
    }

    SECTION("a path that does not exist is dropped on the floor")
    {
      const auto drops = drop_urls(
          ctx, *doc,
          {QUrl::fromLocalFile(scratch_dir("library") + "/no-such-file.fs")});
      CHECK(drops.empty());
    }
  });
}

TEST_CASE("A ISF filter drop carries the shader path, not its bytes", "[gfx][library][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    const QString shader = corpus("isf-control-float.fs");
    const auto drops = drop_urls(ctx, *doc, {QUrl::fromLocalFile(shader)});
    REQUIRE_FALSE(drops.empty());

    bool found = false;
    for(auto& d : drops)
    {
      if(d.creation.key != pkey(UUID_P_FILTER))
        continue;
      found = true;
      // Filter::DropHandler::dropPath passes the RELATIVE path as construction
      // data, which is what Model(duration, init, ...) reopens. An empty one
      // would silently produce the default "Colorize" shader instead.
      CHECK_FALSE(d.creation.customData.isEmpty());
      CHECK(d.creation.customData.endsWith(QStringLiteral("isf-control-float.fs")));
      CHECK_FALSE(d.creation.prettyName.isEmpty());
    }
    CHECK(found);
  });
}

TEST_CASE("Shadertoy URLs that are not shader pages never download", "[gfx][library][gui]")
{
  // Both guards in Gfx::Filter::DropHandler::dropCustom, without a request:
  //  - a shadertoy.com URL whose path is not /view/...
  //  - a /view/ URL with an empty shader id
  // and a non-shadertoy http URL, which must not be treated as a local file.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    for(const char* u :
        {"https://www.shadertoy.com/browse", "https://www.shadertoy.com/view/",
         "https://example.invalid/some/page"})
    {
      INFO(u);
      const auto drops = drop_urls(ctx, *doc, {QUrl{QString::fromUtf8(u)}});
      CHECK_FALSE(has_key(drops, pkey(UUID_P_FILTER)));
    }
  });
}

TEST_CASE("The Filter library scanner rejects raw-raster shaders", "[gfx][library][gui]")
{
  // Gfx::Filter::LibraryHandler::scanPath must NOT claim a shader declaring
  // RAW_RASTER_PIPELINE — that one belongs to the RenderPipeline process, and
  // claiming it would make it open as an ISF filter with an empty program.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* lib = librarian(ctx, UUID_L_FILTER);
    REQUIRE(lib != nullptr);

    const auto plain = corpus("isf-solid-color.fs").toStdString();
    CHECK(lib->scanPath(plain).has_value());

    const auto raster = corpus("raw-raster-basic.fs").toStdString();
    CHECK_FALSE(lib->scanPath(raster).has_value());
  });
}

TEST_CASE(
    "The texture-address drop handler only claims node and message lists",
    "[gfx][library][gui]")
{
  // Gfx::Filter::VideoTextureDropHandler turns a dragged *device address* into
  // a passthrough ISF wired to it. It must not claim plain files: it declares
  // no file extensions, only the two internal mime types.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* h = dropper(ctx, UUID_D_FILTER_TEX);
    REQUIRE(h != nullptr);
    CHECK(h->fileExtensions().isEmpty());
    CHECK(h->mimeTypes().size() == 2);

    // A mime that carries neither of those two formats must yield nothing.
    std::vector<Process::ProcessDropHandler::ProcessDrop> drops;
    QMimeData other;
    other.setText(QStringLiteral("not a node list"));
    h->getCustomDrops(drops, other, doc->context());
    CHECK(drops.empty());

    // FINDING (not asserted, because it ABORTS the process): a QMimeData that
    // DOES declare score::mime::nodelist() but carries an empty payload takes
    // Gfx::Filter::VideoTextureDropHandler::dropCustom straight into
    // Mime<Device::FreeNodeList>::Deserializer::deserialize(), which hands ""
    // to rapidjson and trips its `IsArray()` assertion — SIGABRT, no
    // recoverable error. Reproduce by setting each of h->mimeTypes() to an
    // empty QByteArray here. Left out of the suite on purpose: a SIGABRT kills
    // the whole binary, so it cannot be encoded even as [!shouldfail].
  });
}
