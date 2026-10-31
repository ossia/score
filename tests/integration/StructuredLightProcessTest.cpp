// The Structured Light Calibration process exists in the application and has
// the ports scripts and documents address by name.

#include <QByteArray>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/AppProcess.hpp>

namespace
{
namespace app = score::test::app;
}

TEST_CASE("Structured Light Calibration is created with its ports", "[integration][threedim][structuredlight]")
{
  if(!app::binary_available())
    SKIP("the score application binary was not built");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  auto r = app::run_script(app::write_file(dir, "sl.js", R"JS(
var root = Score.rootInterval();
var p = Score.createProcess(root, "Structured Light Calibration", "");
if(!p) { console.log("NO-PROCESS"); Qt.exit(0); }
var ins = [], outs = [];
for (var i = 0; i < Score.inlets(p); i++) ins.push(Score.portName(Score.inlet(p, i)));
for (var i = 0; i < Score.outlets(p); i++) outs.push(Score.portName(Score.outlet(p, i)));
console.log("[IN] " + ins.join("|"));
console.log("[OUT] " + outs.join("|"));
Score.setValue(Score.inlet(p, "Settle (ms)"), 250);
Score.setValue(Score.inlet(p, "Pattern level"), 0.5);
console.log("[SETTLE] " + Score.inlet(p, "Settle (ms)").value);
Qt.exit(0);
)JS"));
  CHECK_FALSE(r.crashed);
  CHECK(r.exit_code == 0);
  INFO(r.output.toStdString());
  REQUIRE_FALSE(r.output.contains("NO-PROCESS"));
  for(const char* in :
      {"Camera", "Positions", "World from camera", "Projector size", "Mode", "Camera FOV",
       "Camera intrinsics", "Pattern level", "Min stripe px", "Settle (ms)",
       "Frames to average", "Contrast threshold", "Start", "Abort", "Save file", "Save",
       "Load file", "Surface", "Max planes", "Plane threshold (m)", "Phase shift",
       "Phase steps", "Phase period px", "Projector gamma", "Phase min modulation",
       "Intrinsics mode", "Locked intrinsics", "Throw ratio", "Lens shift",
       "Accumulate intrinsics"})
    CHECK((r.output.contains(QStringLiteral("|") + in) || r.output.contains(QStringLiteral("[IN] ") + in)));
  for(const char* out :
      {"Pattern", "XYZ map", "Decoded", "Intrinsics", "Pose m0", "Pose m3",
       "World from projector", "RMS", "Coverage", "Progress", "Status", "Done", "Planes", "Intrinsics (accumulated)"})
    CHECK(r.output.contains(QString::fromLatin1(out)));
  CHECK(r.output.contains("[SETTLE] 250"));
}
