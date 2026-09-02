// =============================================================================
// A9 (the half that matters most) — pointing an avnd raw-file port at a path
// that does not exist must SAY SO.
//
// THE DEFECT. `oscr::loadRawfile` (Crousti/File.hpp) returned a null handle for
// a file it could not open, and all three of its callers are written
//
//     if(auto hdl = loadRawfile(inlet->value(), ctx, has_text, has_mmap)) { ... }
//
// (ExecutorPortSetup.hpp:298 at initialize, :327 on a live control change, and
// GpuUtils.hpp:202 on the GPU path). So a missing file produced NOTHING
// anywhere: no load, no `file_loaded` callback, no `Field::process`, no
// message. The object never learns that anything happened, which is why the
// diagnostic cannot live on the object -- an error OUTPUT PORT on the node
// could only ever report the sub-case where the file was read and its parser
// then refused it.
//
// This is not an edge case. A document written on another machine carries
// absolute paths that do not resolve here; the score corpus addresses assets
// through four different path syntaxes, C:/Users/... among them, and 24 real
// scores use the Asset Loader alone. The fix is at this layer, so it covers
// every avnd raw-file port in score rather than one node.
//
// An UNSET port -- no path at all -- must stay silent: that is not a failure,
// and warning about it would make the real messages worthless.
//
//   ctest -R integration_avnd_file_port_diagnostic --output-on-failure
// =============================================================================
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Crousti/File.hpp>

#include <ossia/network/value/value.hpp>

#include <QtGlobal>

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace
{
std::vector<std::string>& capturedMessages()
{
  static std::vector<std::string> v;
  return v;
}
void messageCapture(QtMsgType, const QMessageLogContext&, const QString& msg)
{
  capturedMessages().push_back(msg.toStdString());
}

bool mentions(std::string_view needle)
{
  for(const auto& m : capturedMessages())
    if(m.find(needle) != std::string::npos)
      return true;
  return false;
}
}

TEST_CASE(
    "a file port pointed at a path that does not exist says so",
    "[avnd][fileport][diagnosability][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    score::Document* doc = score::test::new_document(app);
    REQUIRE(doc != nullptr);
    const score::DocumentContext& ctx = doc->context();

    // Every path syntax the corpus uses, none of which resolves here.
    const char* missing[] = {
        "/nonexistent/score-avnd/model.glb",
        "C:/Users/someone/Desktop/model.glb",
        "C:\\Users\\someone\\Desktop\\model.fbx",
    };

    for(const char* path : missing)
    {
      INFO("path: " << path);
      capturedMessages().clear();
      const auto previous = qInstallMessageHandler(&messageCapture);
      auto hdl = oscr::loadRawfile(
          ossia::value{std::string{path}}, ctx, /*text=*/true, /*mmap=*/false);
      qInstallMessageHandler(previous);

      // The contract that already held: nothing is loaded.
      CHECK(hdl == nullptr);
      // The contract this pins: and the user is told, by name.
      for(const auto& m : capturedMessages())
        INFO("  " << m);
      REQUIRE_FALSE(capturedMessages().empty());
      CHECK(mentions("model."));
    }
  });
}

TEST_CASE(
    "an unset file port stays silent", "[avnd][fileport][diagnosability][gui]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& app) {
    score::Document* doc = score::test::new_document(app);
    REQUIRE(doc != nullptr);
    const score::DocumentContext& ctx = doc->context();

    // A port nobody has filled in: an empty string, and a value that is not a
    // string at all. Neither is a failure; neither may produce a message,
    // otherwise a fresh document would log one line per unset file port and
    // the real diagnostics would be lost in it.
    capturedMessages().clear();
    const auto previous = qInstallMessageHandler(&messageCapture);
    auto empty = oscr::loadRawfile(ossia::value{std::string{}}, ctx, true, false);
    auto none = oscr::loadRawfile(ossia::value{}, ctx, true, false);
    auto wrong_type = oscr::loadRawfile(ossia::value{42}, ctx, true, false);
    qInstallMessageHandler(previous);

    CHECK(empty == nullptr);
    CHECK(none == nullptr);
    CHECK(wrong_type == nullptr);
    for(const auto& m : capturedMessages())
      INFO("  " << m);
    CHECK(capturedMessages().empty());
  });
}
