// Gfx/Video and Gfx/Text process models.
//
// Neither is named by any test. Both are "leaf" processes — no shader to edit —
// so what matters is the property commands a live edit drives (the clip, the
// scale/playback/tonemap/output-format enums, the tempo overrides), each of
// which is an undoable PROPERTY_COMMAND_T, and what the model does with a file
// that is missing or is not decodable.
//
// FINDING pinned here (see "Text process control surface" below):
// Gfx::Text::ProcessExecutorComponent walks `element.inlets()[i]` for a
// hard-coded i in [0, 8) and dereferences the qobject_cast result without a
// null check, while Gfx::Text::Model has no version-upgrade path in its JSON
// writer (unlike Gfx::Images::Model, which explicitly appends the ports newer
// versions added). A document saved by a build whose Text process had fewer
// than 8 inlets therefore reads out of bounds and null-derefs the moment it is
// executed. The test below cannot construct that document through the public
// API, so it pins the invariant the executor silently depends on instead.

#include "GfxProcessDoc.hpp"

#include <Gfx/Graph/Scale.hpp>
#include <Gfx/Text/Process.hpp>
#include <Gfx/Video/Process.hpp>

#include <Process/Dataflow/WidgetInlets.hpp>

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <catch2/catch_test_macros.hpp>

using namespace score::test;
using namespace score::test::gfxproc;

namespace
{
constexpr auto UUID_VIDEO = "32dc5341-7748-4c31-a226-82e6bd685744";
constexpr auto UUID_TEXT = "88bd9718-2a36-42ba-8eab-da5f84e3978e";

/// A short H.264 clip. Prefers the one the media wrapper provisions
/// (SCORE_TEST_MEDIA_DIR), else makes one with ffmpeg, else returns {} and the
/// caller SKIPs — a clip is genuinely optional on a build host.
QString test_clip()
{
  // A clip that EXISTS is not a clip that DECODES.
  //
  // `ffmpeg -y` creates its output before it encodes anything, so a run whose
  // ffmpeg died -- a full disk is how this actually happened, on macOS on
  // 4 Sept -- leaves a zero-byte clip.mp4 behind. That run correctly returned
  // {} and SKIPped, but every LATER run in the same scratch dir hit the
  // existence check below and handed the stub to the decoder, for good:
  // ffprobe says "moov atom not found", makeDecoder() returns null, and the
  // case fails. It read as a backend-specific defect on macOS/OpenGL and is
  // nothing of the kind -- it fails identically on Metal.
  //
  // So: size-check what we hand back, and never leave a non-clip behind.
  const auto usable = [](const QString& path) {
    const QFileInfo fi{path};
    return fi.isFile() && fi.size() > 1024;
  };

  if(const auto d = qEnvironmentVariable("SCORE_TEST_MEDIA_DIR"); !d.isEmpty())
  {
    const QString p = d + QStringLiteral("/h264.mp4");
    if(usable(p))
      return p;
  }

  const QString dir = scratch_dir("video");
  const QString out = dir + QStringLiteral("/clip.mp4");
  if(usable(out))
    return out;
  QFile::remove(out);

  const QString ffmpeg = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
  if(ffmpeg.isEmpty())
    return {};

  QProcess p;
  p.start(
      ffmpeg,
      {"-nostdin", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
       "testsrc=size=160x120:rate=25:duration=1", "-pix_fmt", "yuv420p", "-c:v",
       "libx264", "-preset", "ultrafast", out});
  if(!p.waitForFinished(60000) || p.exitCode() != 0 || !usable(out))
  {
    QFile::remove(out);
    return {};
  }
  return out;
}

/// An .mp4 whose bytes are not a video stream.
QString bogus_clip()
{
  const QString dir = scratch_dir("video");
  const QString out = dir + QStringLiteral("/bogus.mp4");
  QFile f{out};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(QByteArray(4096, 'Z'));
  f.close();
  return out;
}
}

TEST_CASE("Changing the video clip is undoable", "[gfx][video][command][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    const QString first = bogus_clip();
    auto* proc = add_process(ctx, *doc, UUID_VIDEO, first);
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::Video::Model&>(*proc);

    CHECK(m.path() == first);
    CHECK(m.outlets().size() == 1);

    const QString second = first + QStringLiteral(".other.mp4");
    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Gfx::ChangeVideo{m, second});

    CHECK(m.path() == second);
    REQUIRE(stack.canUndo());
    stack.undo();
    CHECK(m.path() == first);
    REQUIRE(stack.canRedo());
    stack.redo();
    CHECK(m.path() == second);
  });
}

TEST_CASE("A missing or undecodable clip never yields a decoder", "[gfx][video][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(
        ctx, *doc, UUID_VIDEO, scratch_dir("video") + QStringLiteral("/nope.mp4"));
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::Video::Model&>(*proc);

    // makeDecoder() is what the executor calls; it must swallow libav's failure
    // rather than throw into the execution thread.
    CHECK(m.makeDecoder() == nullptr);
    CHECK_FALSE(m.absolutePath().isEmpty());

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Gfx::ChangeVideo{m, bogus_clip()});
    CHECK(m.makeDecoder() == nullptr);
  });
}

TEST_CASE("Loading a real clip picks up its duration", "[gfx][video][gui]")
{
  const QString clip = test_clip();
  if(clip.isEmpty())
    SKIP("no test clip: neither SCORE_TEST_MEDIA_DIR nor ffmpeg is available");

  run_in_gui_app([clip](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_VIDEO, clip);
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::Video::Model&>(*proc);

    // setPath() probes the file (Gfx::Video::guessVideoProps) and pushes the
    // stream duration into the process' loop duration. A 1s clip must not
    // leave the default duration behind.
    CHECK(m.loopDuration().impl > 0);
    CHECK(m.loops());
    CHECK(m.makeDecoder() != nullptr);

    // Switching to a file that is not decodable must not resurrect a decoder.
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Gfx::ChangeVideo{m, bogus_clip()});
    CHECK(m.makeDecoder() == nullptr);

    doc->commandStack().undo();
    CHECK(m.path() == clip);
    CHECK(m.makeDecoder() != nullptr);
  });
}

TEST_CASE("Every video playback property is undoable", "[gfx][video][command][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_VIDEO, bogus_clip());
    REQUIRE(proc != nullptr);
    auto& m = static_cast<Gfx::Video::Model&>(*proc);

    score::CommandStack& stack = doc->commandStack();
    CommandDispatcher<> disp{doc->context().commandStack};

    const auto scale0 = m.scaleMode();
    const auto play0 = m.playbackMode();
    const auto tempo0 = m.nativeTempo();
    const auto ignore0 = m.ignoreTempo();

    disp.submit(new Gfx::ChangeVideoScaleMode{m, score::gfx::ScaleMode::Stretch});
    disp.submit(new Gfx::ChangePlaybackMode{m, score::gfx::PlaybackMode::FrameQueue});
    disp.submit(new Gfx::ChangeTempo{m, tempo0 + 37.5});
    disp.submit(new Gfx::ChangeIgnoreTempo{m, !ignore0});

    CHECK(m.scaleMode() == score::gfx::ScaleMode::Stretch);
    CHECK(m.playbackMode() == score::gfx::PlaybackMode::FrameQueue);
    CHECK(m.nativeTempo() == tempo0 + 37.5);
    CHECK(m.ignoreTempo() == !ignore0);

    stack.undo();
    stack.undo();
    stack.undo();
    stack.undo();

    CHECK(m.scaleMode() == scale0);
    CHECK(m.playbackMode() == play0);
    CHECK(m.nativeTempo() == tempo0);
    CHECK(m.ignoreTempo() == ignore0);

    stack.redo();
    stack.redo();
    stack.redo();
    stack.redo();

    CHECK(m.scaleMode() == score::gfx::ScaleMode::Stretch);
    CHECK(m.playbackMode() == score::gfx::PlaybackMode::FrameQueue);
    CHECK(m.ignoreTempo() == !ignore0);
  });
}

TEST_CASE("Text process control surface", "[gfx][text][gui]")
{
  // Gfx::Text::ProcessExecutorComponent indexes inlets()[0..7] unconditionally
  // and calls value() on the qobject_cast result. This pins the two invariants
  // it depends on. If a future edit adds, removes or reorders a Text inlet
  // without touching the executor, this test goes red instead of the
  // application crashing on play.
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_TEXT);
    REQUIRE(proc != nullptr);

    REQUIRE(proc->inlets().size() == 8);
    for(std::size_t i = 0; i < 8; i++)
      CHECK(qobject_cast<Process::ControlInlet*>(proc->inlets()[i]) != nullptr);

    CHECK(inlet_names(*proc)
          == std::vector<QString>{
              "Text", "Font", "Point size", "Opacity", "Position", "Scale X", "Scale Y",
              "Color"});
    CHECK(proc->outlets().size() == 1);
  });
}

TEST_CASE("Text process survives a save / reload round trip", "[gfx][text][gui]")
{
  run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    score::Document* doc = new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* proc = add_process(ctx, *doc, UUID_TEXT);
    REQUIRE(proc != nullptr);
    const auto names = inlet_names(*proc);

    score::Document* reloaded = reload_via_bytes(ctx, *doc);
    REQUIRE(reloaded != nullptr);

    Process::ProcessModel* back{};
    for(auto& p : base_interval(*reloaded).processes)
      if(p.concreteKey()
         == UuidKey<Process::ProcessModel>::fromString(QString::fromUtf8(UUID_TEXT)))
        back = &p;

    REQUIRE(back != nullptr);
    // The executor's hard-coded 8 must hold after deserialization too — this is
    // the path a document from an older build takes.
    CHECK(back->inlets().size() == 8);
    CHECK(inlet_names(*back) == names);
  });
}
