// The library and drop layer around the Gfx processes: which file lands on
// which process when it is dragged into the score, and what the Shadertoy
// import does with a URL.
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

#include <Gfx/Filter/Library.hpp>
#include <Gfx/Filter/Process.hpp>
#include <Process/ProcessList.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>
#include <Gfx/WindowDevice.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/command/Dispatchers/RuntimeDispatcher.hpp>
#include <Process/Drop/ProcessDropHandler.hpp>

#include <Library/LibraryInterface.hpp>

#include <QFileInfo>
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
constexpr auto UUID_P_VSA = "ea13ed06-d21c-4c84-8d0f-83ce0027b81c";
constexpr auto UUID_P_GEOMFILTER = "27d3cc85-a4b0-4924-8fde-71c337b40f59";
// score_plugin_threedim; absent from a build without it.
constexpr auto UUID_P_RASTER = "dbfc2101-40d7-4807-8804-571e88992e7e";

// Drop handlers
constexpr auto UUID_D_FILTER = "d1e16bba-4c53-4d24-8b6b-71b94daef68d";
constexpr auto UUID_D_FILTER_TEX = "e9bf6cf8-c872-4638-b98a-ed76edc8e2dd";
constexpr auto UUID_D_CSF = "b3adba36-29cc-45b4-bea3-5a2a89458a48";
constexpr auto UUID_D_IMAGES = "f37aa176-d8be-45bc-b833-d014efba6157";
constexpr auto UUID_D_VIDEO = "12d1ed39-0fac-43da-8520-b7e32f9fad7d";
constexpr auto UUID_D_VSA = "78977726-e594-4d78-a9b2-09fc0f41afe3";
constexpr auto UUID_D_GEOMFILTER = "e3a8ec68-262a-419a-b4bb-a7e0400f4c24";
constexpr auto UUID_D_RASTER = "3b0a1a6a-6e6f-4a35-9c4f-3fa2d0d07a09";

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

//! Whether the plugin that owns the raw-raster process is in this build.
//! Keyed on the process factory, not on the drop handler: the handler is what
//! these cases are testing, so guarding on it would turn a regression into a
//! silent skip.
bool hasRawRasterProcess(const score::GUIApplicationContext& ctx)
{
  return ctx.interfaces<Process::ProcessFactoryList>().get(
             UuidKey<Process::ProcessModel>::fromString(
                 QString::fromUtf8(UUID_P_RASTER)))
         != nullptr;
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

/// Copy a corpus shader to `dest`, creating the folders on the way.
void copy_corpus(const char* name, const QString& dest)
{
  QDir{}.mkpath(QFileInfo{dest}.absolutePath());
  QFile::remove(dest);
  REQUIRE(QFile::copy(corpus(name), dest));
}

const Process::ProcessDropHandler::ProcessDrop* drop_for(
    const std::vector<Process::ProcessDropHandler::ProcessDrop>& drops,
    const UuidKey<Process::ProcessModel>& k)
{
  for(auto& d : drops)
    if(d.creation.key == k)
      return &d;
  return nullptr;
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

// Gfx::CSF::DropHandler (Gfx/CSF/Library.hpp) has to appear in
// score_plugin_gfx.cpp's FW<Process::ProcessDropHandler, …> list, not only
// Gfx::CSF::LibraryHandler in the FW<Library::LibraryInterface, …> list below
// it: with the library entry alone a .cs compute shader shows up in the library
// tree and cannot be dragged into a score at all.
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
  // Handlers that sort their files out by reading them may share an extension
  // (see "A dropped shader goes to the process of its declared family"); these
  // three tell media apart by extension alone, so an overlap between them would
  // send a file to a process that cannot read it.
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

    // A QMimeData that DOES declare score::mime::nodelist() but carries a
    // malformed payload cannot be exercised here: it goes through
    // Gfx::Filter::VideoTextureDropHandler::dropCustom into
    // Mime<Device::FreeNodeList>::Deserializer::deserialize(), where a
    // rapidjson assertion would SIGABRT the whole binary.
    // tests/gfx/GfxDropEmptyNodelistAbort.cpp runs those drops in a FORKED
    // CHILD and asserts on the wait status, covering both levels -- the payload
    // itself and the array's CONTENTS. Add new malformed-mime cases there.
  });
}

TEST_CASE(
    "A dropped texture address is wired to the end of the passthrough it belongs on",
    "[gfx][library][gui]")
{
  // Dropping a camera builds a passthrough that reads from it; dropping a
  // window has to build one that writes to it. Both are texture addresses and
  // the handler took them for the same thing, so a screen was asked for a
  // picture it does not have.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* base = dropper(ctx, UUID_D_FILTER_TEX);
    REQUIRE(base != nullptr);
    auto* h = dynamic_cast<const Gfx::Filter::VideoTextureDropHandler*>(base);
    REQUIRE(h != nullptr);

    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();

    Device::DeviceSettings set;
    set.name = "Win";
    set.protocol = Gfx::WindowProtocolFactory::static_concreteKey();
    Gfx::WindowSettings ws;
    ws.mode = Gfx::WindowMode::Single;
    set.deviceSpecificSettings = QVariant::fromValue(ws);
    CommandDispatcher<>{doc->context().commandStack}.submit(
        new Explorer::Command::LoadDevice{plug, set});

    const auto& devices = plug.list();
    State::Address win{"Win", {}};

    // It is a texture either way; which way it goes is the new question.
    CHECK(h->isTexture(win, devices));
    CHECK(h->isTextureSink(win, devices));

    // Nothing of that name is not a sink, and neither is an address on a
    // device that is not a texture output at all.
    State::Address nothing{"NoSuchDevice", {}};
    CHECK_FALSE(h->isTextureSink(nothing, devices));

    // And the drop it builds puts the window on the end that writes.
    std::vector<Process::ProcessDropHandler::ProcessDrop> drops;
    REQUIRE(h->create(drops, {win}, devices));
    REQUIRE(drops.size() == 1);
    REQUIRE(bool(drops[0].setup));

    auto& interval
        = score::IDocument::get<Scenario::ScenarioDocumentModel>(*doc).baseInterval();
    auto& factories = ctx.interfaces<Process::ProcessFactoryList>();
    auto* fact = factories.get(drops[0].creation.key);
    REQUIRE(fact != nullptr);

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit<Scenario::Command::AddOnlyProcessToInterval>(
        interval, fact->concreteKey(), QString{}, QPointF{});

    Process::ProcessModel* made = nullptr;
    for(auto& pr : interval.processes)
      if(pr.concreteKey() == drops[0].creation.key)
        made = &pr;
    REQUIRE(made != nullptr);
    REQUIRE(!made->outlets().empty());
    REQUIRE(!made->inlets().empty());

    CommandDispatcher<> setupDisp{doc->context().commandStack};
    score::Dispatcher_T<CommandDispatcher<>> d{setupDisp};
    drops[0].setup(*made, d);

    CHECK(made->outlets().front()->address().address == win);
    CHECK(made->inlets().front()->address().address != win);
  });
}

TEST_CASE(
    "A dropped shader goes to the process of its declared family",
    "[gfx][library][gui]")
{
  // Four shader families share ".fs"/".frag"/".glsl"/".vs" between them, and
  // ProcessDropHandlerList used to keep exactly ONE handler per extension: the
  // survivor was whichever the interface list happened to hash last, and every
  // other family became undroppable. Now every handler registered for the
  // extension is asked, and each one reads the file's MODE to know whether the
  // file is its own.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    SECTION("a raw-raster fragment shader is a render pipeline, never an ISF filter")
    {
      if(!hasRawRasterProcess(ctx))
        SKIP("score_plugin_threedim is not part of this build");
      REQUIRE(dropper(ctx, UUID_D_RASTER) != nullptr);

      const auto drops
          = drop_urls(ctx, *doc, {QUrl::fromLocalFile(corpus("raw-raster-basic.fs"))});
      REQUIRE(drops.size() == 1);
      CHECK(has_key(drops, pkey(UUID_P_RASTER)));
      CHECK_FALSE(has_key(drops, pkey(UUID_P_FILTER)));
    }

    SECTION("a geometry filter and a plain shader both claim .glsl")
    {
      const auto geom = drop_urls(
          ctx, *doc, {QUrl::fromLocalFile(corpus("syn-geofilter-shift.glsl"))});
      REQUIRE(geom.size() == 1);
      CHECK(has_key(geom, pkey(UUID_P_GEOMFILTER)));

      const QString plain = file_with(
          "plain-filter.glsl", "void main() { gl_FragColor = vec4(1.); }");
      const auto isf = drop_urls(ctx, *doc, {QUrl::fromLocalFile(plain)});
      REQUIRE(isf.size() == 1);
      CHECK(has_key(isf, pkey(UUID_P_FILTER)));
    }

    SECTION("a compute shader is a CSF process")
    {
      const auto drops
          = drop_urls(ctx, *doc, {QUrl::fromLocalFile(corpus("csf-gradient-y.cs"))});
      REQUIRE(drops.size() == 1);
      CHECK(has_key(drops, pkey(UUID_P_CSF)));
    }

    SECTION("a vertex-shader-art shader is a VSA process")
    {
      const auto drops
          = drop_urls(ctx, *doc, {QUrl::fromLocalFile(corpus("vsa-points.vs"))});
      REQUIRE(drops.size() == 1);
      CHECK(has_key(drops, pkey(UUID_P_VSA)));
    }
  });
}

TEST_CASE("A fragment shader and its vertex shader are one process", "[gfx][library][gui]")
{
  // The .vs of a raw-raster or ISF shader is a stage of that shader, not a
  // process: dropping the pair must not also create a Vertex Shader Art
  // process, and neither must dropping the .vs on its own.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    const QUrl fs = QUrl::fromLocalFile(corpus("raw-raster-basic.fs"));
    const QUrl vs = QUrl::fromLocalFile(corpus("raw-raster-basic.vs"));

    SECTION("the companion vertex shader alone yields nothing")
    {
      const auto drops = drop_urls(ctx, *doc, {vs});
      CHECK_FALSE(has_key(drops, pkey(UUID_P_VSA)));
      CHECK(drops.empty());
    }

    SECTION("the pair yields a single process")
    {
      if(!hasRawRasterProcess(ctx))
        SKIP("score_plugin_threedim is not part of this build");
      REQUIRE(dropper(ctx, UUID_D_RASTER) != nullptr);

      const auto drops = drop_urls(ctx, *doc, {fs, vs});
      CHECK(drops.size() == 1);
      CHECK(has_key(drops, pkey(UUID_P_RASTER)));
      CHECK_FALSE(has_key(drops, pkey(UUID_P_VSA)));
    }
  });
}

TEST_CASE("A raw-raster drop carries both shader stages", "[gfx][library][gui]")
{
  // baseName() truncates at the FIRST dot, so the vertex shader of
  // `my.shader.fs` was looked for as `my.vs` and the pipeline came up with an
  // empty vertex stage. The properties are read through the meta-object so the
  // test does not need score_plugin_threedim's headers.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    if(!hasRawRasterProcess(ctx))
      SKIP("score_plugin_threedim is not part of this build");
    REQUIRE(dropper(ctx, UUID_D_RASTER) != nullptr);

    const QString dir = scratch_dir("raster-pair");
    copy_corpus("raw-raster-basic.fs", dir + "/my.shader.fs");
    copy_corpus("raw-raster-basic.vs", dir + "/my.shader.vs");

    const auto drops
        = drop_urls(ctx, *doc, {QUrl::fromLocalFile(dir + "/my.shader.fs")});
    const auto* d = drop_for(drops, pkey(UUID_P_RASTER));
    REQUIRE(d != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_P_RASTER, d->creation.customData);
    REQUIRE(proc != nullptr);
    CHECK(proc->property("fragment").toString().contains("isf_FragColor = v_color"));
    CHECK(proc->property("vertex").toString().contains("gl_Position"));
  });
}

TEST_CASE(
    "A shader dropped from inside the document folder still opens",
    "[gfx][library][gui]")
{
  // The real-world case: the shader lives next to the .score file, so the drop
  // layer relativizes its path to "<PROJECT>:..." -- which is not a path any
  // QFile can open. The process must resolve it back, exactly as loading a
  // saved document does.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    const QString folder = scratch_dir("project-drop");
    QDir{folder}.removeRecursively();
    QDir{}.mkpath(folder);

    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);
    REQUIRE(ctx.docManager.saveDocumentAs(*doc, folder + "/project.score"));

    const QString shader = folder + "/Shaders/dropped.fs";
    copy_corpus("isf-control-float.fs", shader);

    const auto drops = drop_urls(ctx, *doc, {QUrl::fromLocalFile(shader)});
    const auto* d = drop_for(drops, pkey(UUID_P_FILTER));
    REQUIRE(d != nullptr);
    // Portable, as everything else a document stores: it is the process' job
    // to resolve it.
    CHECK(d->creation.customData.startsWith(QStringLiteral("<PROJECT>:")));

    auto* proc = add_process(ctx, *doc, UUID_P_FILTER, d->creation.customData);
    REQUIRE(proc != nullptr);
    auto* filter = safe_cast<Gfx::Filter::Model*>(proc);
    CHECK(filter->fragment().contains("gl_FragColor = vec4(vec3(level), 1.0)"));
  });
}
