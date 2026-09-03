// Corpus tester for score documents: runs ONE .score document through the
// exact load / serialize / execute / render paths the application uses and
// reports a machine-readable verdict on stdout. Meant to be driven over a
// whole corpus of real user documents ($SCORE_CORPUS_DIR) by
// run-score-corpus.sh, one process per document so a crash or hang condemns
// only that document — the same contract as VideoCorpusTester.cpp and
// run-corpus.sh, which this pair is modelled on (spec case P1-15).
//
// Usage:
//   score_corpus_tester <file.score> [--seconds N] [--no-render]
//
// Per document, in this process:
//  1. Pre-scan the raw JSON for process uuids and compare them against the
//     registered Process::ProcessFactoryList. Any unknown uuid is verdict
//     UNKNOWN_UUID:<uuid> and the document is NOT loaded: on load, an unknown
//     process is silently dropped (ProcessFactoryList::loadMissing is
//     SCORE_TODO -> nullptr, see IntervalModelSerialization.cpp:199), which
//     would both hide the reference and break the round-trip check. ~20 of
//     the user's real documents reference processes that exist in no branch;
//     for those, UNKNOWN_UUID is the correct, *expected* baseline verdict.
//  2. Load the document through the application path:
//     DocumentManager::loadFile (DocumentManager.cpp:615). Failure -> LOADFAIL.
//  3. Round-trip: Document::saveAsJson, reload that byte-for-byte through
//     DocumentManager::loadDocument, saveAsJson again, compare. A divergence
//     is verdict ROUNDTRIP with the first differing byte offset in the note.
//  4. Build the graphs by playing, exactly as --autoplay does
//     (Engine::ApplicationPlugin::afterStartup ->
//     ExecutionController::request_play_local(true)). If after the wait the
//     execution never came up (Execution::DocumentPlugin::isPlaying() false),
//     the graph did not build -> GRAPHFAIL.
//  5. Render leg: every Gfx::WindowDevice was forced offscreen via
//     SCORE_FORCE_OFFSCREEN_WINDOW (set from the pre-scan device names before
//     the app boots — the WindowDevice matcher is an exact name list, see
//     WindowDevice.cpp shouldForceOffscreen). Grab each window with
//     WindowDevice::grabTo, the same call Score.device(...).grabTo uses in
//     integration/scene-js-sweep.sh. Any non-blank grab -> OK; texture
//     outlets present but every grab blank or unwritten -> BLANK. A document
//     with no texture outlet legitimately has no gfx output -> OK with a note.
//
// Verdicts: OK / UNKNOWN_UUID:<uuid> / LOADFAIL / ROUNDTRIP / GRAPHFAIL /
// BLANK — plus CRASH_SIG<n> / TIMEOUT which are synthesized by the driver
// (run-score-corpus.sh) from the process's signal exit / timeout, exactly as
// run-corpus.sh does for the video tester.
//
// Exit codes: 0 verdict printed, 2 usage error. Crashes and hangs are the
// driver's to detect. Like ObjectGallery.cpp, the success path leaves through
// std::_Exit after printing: the gfx teardown path crashes on some drivers
// AFTER everything under test already happened (scene-js-sweep.sh documents
// the same SIGSEGV-after-grab), and that must not turn every OK into a
// CRASH_SIG11 report line.

#include <Process/Process.hpp>
#include <Process/ProcessList.hpp>

#include <Engine/ApplicationPlugin.hpp>
#include <Execution/DocumentPlugin.hpp>
#include <Execution/ExecutionController.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Gfx/TexturePort.hpp>
#include <Gfx/WindowDevice.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/plugins/UuidKey.hpp>

#include <core/application/MinimalApplication.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/context.hpp>
#include <ossia/detail/thread.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLocale>
#include <QTimer>

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

namespace
{

// ---------------------------------------------------------------------------
// Verdict record, one JSON line on stdout. Field order matches the video
// tester's emit() so run-score-corpus.sh can build its summary with the same
// '"mode":"...","file":"...","status":"..."' greps run-corpus.sh uses.
// ---------------------------------------------------------------------------
struct Verdict
{
  std::string status;
  std::string note;
  int process_uuids = -1;   // process uuids found in the raw JSON
  int unknown_uuids = -1;   // of those, not in ProcessFactoryList
  int texture_outlets = -1; // Gfx::TextureOutlet count in the loaded document
  int windows = -1;         // Gfx::WindowDevice count in the device list
  std::string roundtrip;    // "stable" / "differs" / "skipped" / ""
};

std::string json_escape(const std::string& s)
{
  std::string r;
  r.reserve(s.size());
  for(char c : s)
  {
    if(c == '"' || c == '\\')
      r += '\\';
    if((unsigned char)c < 0x20)
    {
      r += ' ';
      continue;
    }
    r += c;
  }
  return r;
}

void note_append(std::string& note, const std::string& what)
{
  if(!note.empty())
    note += "; ";
  note += what;
}

void emit(const std::string& file, const Verdict& v)
{
  std::printf(
      "{\"mode\":\"scene\",\"file\":\"%s\",\"status\":\"%s\",\"process_uuids\":%d,"
      "\"unknown_uuids\":%d,\"texture_outlets\":%d,\"windows\":%d,"
      "\"roundtrip\":\"%s\",\"note\":\"%s\"}\n",
      json_escape(file).c_str(), v.status.c_str(), v.process_uuids, v.unknown_uuids,
      v.texture_outlets, v.windows, json_escape(v.roundtrip).c_str(),
      json_escape(v.note).c_str());
  std::fflush(stdout);
}

[[noreturn]] void finish(const std::string& file, Verdict v)
{
  // A diverging round-trip is the weakest verdict: it is reported only when
  // nothing worse was found. See the round-trip section for why it does not
  // short-circuit.
  if(v.status == "OK" && v.roundtrip == "differs")
    v.status = "ROUNDTRIP";
  emit(file, v);
  // Hard exit: everything under test already ran. See the file header for why
  // a clean teardown is not attempted (same rationale as ObjectGallery.cpp).
  std::_Exit(0);
}

// ---------------------------------------------------------------------------
// Raw-JSON pre-scan: process uuids and device names, without loading anything.
// ---------------------------------------------------------------------------
struct Prescan
{
  bool json = false;               // false: .scorebin or unparseable
  std::set<QString> process_uuids; // lowercased textual uuids
  QStringList device_names;        // every "Name" of a device settings object
};

void prescan_walk(const QJsonValue& val, Prescan& out, bool in_processes)
{
  if(val.isObject())
  {
    const QJsonObject obj = val.toObject();
    // A direct element of a "Processes" array carries the process factory
    // uuid. (Nested processes — a Scenario's intervals' racks — are reached
    // because the recursion below descends into everything.)
    if(in_processes)
    {
      const auto u = obj.value(QStringLiteral("uuid"));
      if(u.isString())
        out.process_uuids.insert(u.toString().toLower());
    }
    // A device settings object: {"Name": ..., "Protocol": <uuid>, ...}.
    // Collect every name; only WindowDevice consults the offscreen list, so
    // over-collecting is harmless.
    if(obj.contains(QStringLiteral("Protocol")) && obj.value(QStringLiteral("Name")).isString())
      out.device_names.push_back(obj.value(QStringLiteral("Name")).toString());

    for(auto it = obj.begin(); it != obj.end(); ++it)
      prescan_walk(it.value(), out, it.key() == QStringLiteral("Processes"));
  }
  else if(val.isArray())
  {
    for(const auto& elem : val.toArray())
      prescan_walk(elem, out, in_processes);
  }
}

Prescan prescan(const QByteArray& bytes)
{
  Prescan out;
  QJsonParseError err{};
  const auto doc = QJsonDocument::fromJson(bytes, &err);
  if(err.error != QJsonParseError::NoError || !doc.isObject())
    return out; // .scorebin (or damaged): loadFile still handles it below.
  out.json = true;
  prescan_walk(doc.object(), out, false);
  return out;
}

// ---------------------------------------------------------------------------
// Blank test, same sampling as ObjectGallery::non_blank: more than one
// distinct colour means something actually rendered.
// ---------------------------------------------------------------------------
bool non_blank(const QImage& img)
{
  if(img.isNull() || img.width() < 2 || img.height() < 2)
    return false;
  const QRgb first = img.pixel(0, 0);
  for(int y = 0; y < img.height(); y += 4)
    for(int x = 0; x < img.width(); x += 4)
      if(img.pixel(x, y) != first)
        return true;
  return false;
}

// ---------------------------------------------------------------------------
// Hermetic environment, mirroring score::test::prepare_test_environment
// (tests/fixtures/score_test/App.hpp) — inlined so this target does not need
// the fixtures interface library, exactly as VideoCorpusTester carries its own
// bootstrap. The config dir is per-pid: run-score-corpus.sh runs several
// testers in parallel and they must not share QSettings storage.
// ---------------------------------------------------------------------------
//! A windowing system the RHI can get a GL/Vulkan context from. Same test as
//! tests/integration/GfxNestedIntervalTest.cpp's ready().
bool has_display()
{
  if(qEnvironmentVariable("QT_QPA_PLATFORM") == QStringLiteral("offscreen"))
    return false;
#if defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
  return qEnvironmentVariableIsSet("DISPLAY")
         || qEnvironmentVariableIsSet("WAYLAND_DISPLAY");
#else
  return true;
#endif
}

void prepare_environment()
{
#if defined(SCORE_TEST_BINARY_DIR)
  // score::PluginLoader::pluginsDir() probes "<cwd>/plugins", and this
  // executable does not live next to <build>/plugins the way the application
  // binary does. Without this anchor the app boots with ZERO plug-ins and the
  // first ctx.interfaces<Process::ProcessFactoryList>() hits SCORE_ABORT --
  // measured: SIGABRT at ApplicationComponents.hpp:187 before any document is
  // touched. Same fix, same reason, as
  // score::test::prepare_test_environment (tests/fixtures/score_test/App.hpp:44-53).
  // The document path is made absolute in main() before this runs.
  if(!QDir{QStringLiteral("plugins")}.exists())
    QDir::setCurrent(QStringLiteral(SCORE_TEST_BINARY_DIR));
#endif

  // The offscreen QPA has NO GL: score::gfx::RenderList::init's
  // SCORE_ASSERT(m_emptyTexture->create()) (RenderList.cpp:115) traps as soon
  // as any document builds a render list -- measured, SIGTRAP on
  // demo-naos.score. So only force offscreen when there is genuinely no
  // display; with one, the real platform is used and each WindowDevice is
  // still headless because SCORE_FORCE_OFFSCREEN_WINDOW names it (set from the
  // pre-scan in main()). Without a display the render leg is skipped rather
  // than trapping -- see has_display() at the call site.
  if(!qEnvironmentVariableIsSet("QT_QPA_PLATFORM") && !has_display())
    qputenv("QT_QPA_PLATFORM", "offscreen");
  // The package manager's "Download the user library?" question is a modal
  // QDialog::exec(); with nobody to answer it the event loop wedges.
  if(!qEnvironmentVariableIsSet("SCORE_SANITIZE_SKIP_CHECKS"))
    qputenv("SCORE_SANITIZE_SKIP_CHECKS", "1");
  if(!qEnvironmentVariableIsSet("SCORE_AUDIO_BACKEND"))
    qputenv("SCORE_AUDIO_BACKEND", "dummy");
  if(!qEnvironmentVariableIsSet("XDG_CONFIG_HOME"))
  {
    const QString cfg = QDir::tempPath() + "/score-corpus-tests/config."
                        + QString::number(QCoreApplication::applicationPid());
    QDir{}.mkpath(cfg);
    qputenv("XDG_CONFIG_HOME", cfg.toUtf8());
  }
  QCoreApplication::setOrganizationName("ossia");
  QCoreApplication::setOrganizationDomain("ossia.io");
  // "score", NOT "score-corpus-tester". The package/library search path is
  // derived from QStandardPaths::DocumentsLocation + organization + APPLICATION
  // NAME, so a distinct name pointed this at
  //   ~/Documents/ossia/score-corpus-tester/packages
  // which does not exist, and every document whose shader has an #include
  // failed to compile. The process then kept a default-constructed
  // ProcessedProgram: no RawRaster mode, no geometry port, the scene edge onto
  // it refused as out-of-range, an empty render list, and finally
  // "grabTo: nothing rendered" -- reported as status BLANK for a document that
  // renders perfectly in the application. Measured on
  // instanced-helmets-manual-expression.score: 'Shader include not found:
  // "openpbr.h" (searched: .../score-corpus-tester/packages ...)'.
  //
  // Config isolation does NOT depend on this name: it comes from the
  // XDG_CONFIG_HOME redirect above, which DocumentsLocation ignores. So the
  // tester keeps its private settings and gains the user's real packages.
  QCoreApplication::setApplicationName("score");
}

// ---------------------------------------------------------------------------
// The actual per-document run. Called once on the event loop; never returns.
// ---------------------------------------------------------------------------
void run_document(
    const QString& path, const Prescan& scan, int seconds, bool no_render)
{
  const std::string file = path.toStdString();
  const auto& ctx = score::GUIAppContext();
  Verdict v;

  // -- 1. Unknown process uuids ---------------------------------------------
  if(scan.json)
  {
    std::set<QString> known;
    for(auto& factory : ctx.interfaces<Process::ProcessFactoryList>())
      known.insert(QString::fromUtf8(
                       score::uuids::toByteArray(factory.concreteKey().impl()))
                       .toLower());

    v.process_uuids = int(scan.process_uuids.size());
    QStringList unknown;
    for(const auto& u : scan.process_uuids)
      if(known.find(u) == known.end())
        unknown.push_back(u);
    v.unknown_uuids = unknown.size();

    if(!unknown.isEmpty())
    {
      v.status = "UNKNOWN_UUID:" + unknown.front().toStdString();
      note_append(
          v.note, "unknown process uuids: " + unknown.join(", ").toStdString());
      note_append(
          v.note, "load not attempted: unknown processes are dropped silently "
                  "on load (ProcessFactoryList::loadMissing)");
      finish(file, v);
    }
  }
  else
  {
    note_append(v.note, "not JSON (scorebin?): uuid prescan skipped");
  }

  // -- 2. Load through the application path ---------------------------------
  score::Document* doc{};
  try
  {
    doc = ctx.docManager.loadFile(ctx, path);
  }
  catch(const std::exception& e)
  {
    v.status = "LOADFAIL";
    note_append(v.note, std::string{"exception: "} + e.what());
    finish(file, v);
  }
  catch(...)
  {
    v.status = "LOADFAIL";
    note_append(v.note, "unknown exception during load");
    finish(file, v);
  }
  if(!doc)
  {
    v.status = "LOADFAIL";
    note_append(v.note, "DocumentManager::loadFile returned null");
    finish(file, v);
  }
  QApplication::processEvents();
  QApplication::processEvents();

  // -- 3. JSON round-trip: save -> load -> save must be a fixed point --------
  v.roundtrip = "skipped";
  if(scan.json)
  {
    try
    {
      JSONReader r1;
      r1.buffer.Reserve(1024 * 1024 * 16);
      doc->saveAsJson(r1);
      const QByteArray pass1{r1.buffer.GetString(), (int)r1.buffer.GetSize()};

      auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
      auto* reloaded = ctx.docManager.loadDocument(
          ctx, QStringLiteral("corpus-roundtrip"), pass1, JSONObject::type(),
          *delegates.begin());
      QApplication::processEvents();
      QApplication::processEvents();
      if(!reloaded)
      {
        v.status = "ROUNDTRIP";
        v.roundtrip = "differs";
        note_append(v.note, "reloading the first-pass JSON failed");
        finish(file, v);
      }

      JSONReader r2;
      r2.buffer.Reserve(1024 * 1024 * 16);
      reloaded->saveAsJson(r2);
      const QByteArray pass2{r2.buffer.GetString(), (int)r2.buffer.GetSize()};

      ctx.docManager.forceCloseDocument(ctx, *reloaded);
      QApplication::processEvents();
      // Play acts on the *current* document; put the original back.
      ctx.docManager.setCurrentDocument(ctx, doc);

      if(pass1 != pass2)
      {
        int off = 0;
        const int n = std::min(pass1.size(), pass2.size());
        while(off < n && pass1[off] == pass2[off])
          ++off;
        v.roundtrip = "differs";
        note_append(
            v.note, "serializeAsJson not stable: first divergence at byte "
                        + std::to_string(off) + " (" + std::to_string(pass1.size())
                        + " vs " + std::to_string(pass2.size()) + " bytes)");
        // NOT a finish(): the round-trip is a WEAKER signal than the graph
        // build, and it is currently non-deterministic tree-wide (the
        // ScenarioContentRoundtrip pin in LEDGER-DEFECT-FIXES.md has 2 of its
        // 3 sources still open: view-geometry doubles recomputed on layout,
        // and a random tail). Short-circuiting here would hide the render leg
        // for nearly every document. The verdict is only reported as
        // ROUNDTRIP if nothing worse is found downstream.
      }
      else
      {
        v.roundtrip = "stable";
      }
    }
    catch(const std::exception& e)
    {
      v.status = "ROUNDTRIP";
      v.roundtrip = "differs";
      note_append(v.note, std::string{"exception during round-trip: "} + e.what());
      finish(file, v);
    }
  }

  // -- 4 + 5. Build the graphs by playing; then grab a frame -----------------
  const auto outlets = doc->model().findChildren<Gfx::TextureOutlet*>();
  v.texture_outlets = int(outlets.size());

  auto& eng = ctx.guiApplicationPlugin<Engine::ApplicationPlugin>();
  // Same entry point as --autoplay (Engine::ApplicationPlugin::afterStartup).
  QTimer::singleShot(100, [&eng] { eng.execution().request_play_local(true); });

  QTimer::singleShot(seconds * 1000, [&ctx, doc, file, v, no_render]() mutable {
    auto* exec = doc->context().findPlugin<Execution::DocumentPlugin>();
    if(!exec || !exec->isPlaying())
    {
      v.status = "GRAPHFAIL";
      note_append(
          v.note, exec ? "execution did not start (graph build failed)"
                       : "no execution document plugin");
      finish(file, v);
    }

    if(v.texture_outlets == 0)
    {
      v.status = "OK";
      note_append(v.note, "no gfx output; execution graph built");
      finish(file, v);
    }
    if(no_render)
    {
      v.status = "OK";
      note_append(v.note, "render leg skipped (--no-render); graphs built");
      finish(file, v);
    }
    if(!has_display())
    {
      v.status = "OK";
      note_append(v.note, "render leg skipped (no display); graphs built");
      finish(file, v);
    }

    // Grab every window device (all were forced offscreen before boot).
    int windows = 0, grabbed = 0, content = 0;
    auto& devices = doc->context().plugin<Explorer::DeviceDocumentPlugin>().list();
    for(auto* d : devices.devices())
    {
      auto* wd = qobject_cast<Gfx::WindowDevice*>(d);
      if(!wd)
        continue;
      ++windows;
      const QString png = QDir::tempPath() + "/score_corpus_grab."
                          + QString::number(QCoreApplication::applicationPid())
                          + "." + QString::number(windows) + ".png";
      QFile::remove(png);
      // grabFrame(n, path) drives n frames through the gfx context and THEN
      // reads the shared readback (WindowDevice.cpp:234-238); a bare grabTo
      // reads whatever the last render clock tick left there, which for a
      // just-started document is often still the cleared buffer.
      wd->grabFrame(2, png);
      QApplication::processEvents();
      const QImage img{png};
      if(!img.isNull())
        ++grabbed;
      if(non_blank(img))
        ++content;
      QFile::remove(png);
    }
    v.windows = windows;

    if(windows == 0)
    {
      // Texture outlets exist but nothing routes to a window (Spout/NDI/
      // texture-less sinks): the graph built and played, which is the
      // assertion; there is no window to read pixels back from.
      v.status = "OK";
      note_append(v.note, "graphs built; no window device to grab");
      finish(file, v);
    }
    if(content > 0)
    {
      v.status = "OK";
      note_append(
          v.note, std::to_string(content) + "/" + std::to_string(windows)
                      + " windows non-blank");
      finish(file, v);
    }
    v.status = "BLANK";
    note_append(
        v.note, std::to_string(grabbed) + "/" + std::to_string(windows)
                    + " windows grabbed, all blank");
    finish(file, v);
  });
}

} // namespace

int main(int argc, char** argv)
{
  QString file;
  int seconds = 6; // graph build + first frames before the grab, as
                   // scene-js-sweep.sh's GRAB_DELAY
  bool no_render = false;
  for(int i = 1; i < argc; i++)
  {
    const std::string a = argv[i];
    if(a == "--seconds" && i + 1 < argc)
      seconds = std::max(1, atoi(argv[++i]));
    else if(a == "--no-render")
      no_render = true;
    else
      file = QString::fromLocal8Bit(argv[i]);
  }
  if(file.isEmpty())
  {
    std::fprintf(stderr, "usage: %s [--seconds N] [--no-render] <file.score>\n", argv[0]);
    return 2;
  }

  // Absolute BEFORE prepare_environment(), which may chdir to the build root.
  file = QFileInfo{file}.absoluteFilePath();

  QByteArray bytes;
  {
    QFile f{file};
    if(!f.open(QIODevice::ReadOnly))
    {
      std::fprintf(stderr, "cannot read: %s\n", file.toUtf8().constData());
      return 2;
    }
    bytes = f.readAll();
  }

  // The pre-scan runs before the app boots: SCORE_FORCE_OFFSCREEN_WINDOW must
  // name each window device of THIS document (exact-name list, cached on first
  // use) before any WindowDevice is created during document load.
  const Prescan scan = prescan(bytes);
  if(!scan.device_names.isEmpty() && !qEnvironmentVariableIsSet("SCORE_FORCE_OFFSCREEN_WINDOW"))
    qputenv("SCORE_FORCE_OFFSCREEN_WINDOW", scan.device_names.join(',').toUtf8());

  prepare_environment();

  QLocale::setDefault(QLocale::C);
  std::setlocale(LC_ALL, "C");

  // Same bootstrap as the application and ObjectGallery: pin the UI thread and
  // register the Ossia QML types before any document loads a JS process.
  ossia::context ossia_ctx;

  score::MinimalGUIApplication app{argc, argv, /*show=*/false};

  QMetaObject::invokeMethod(
      &app, [&file, &scan, seconds, no_render] {
    run_document(file, scan, seconds, no_render);
      },
      Qt::QueuedConnection);
  return app.exec();
}
