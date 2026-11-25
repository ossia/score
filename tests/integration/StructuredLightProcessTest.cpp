// The Structured Light Calibration process exists in the application and has
// the ports scripts and documents address by name.

#include <QByteArray>
#include <QStringList>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/AppProcess.hpp>

namespace
{
namespace app = score::test::app;
}

namespace
{
QStringList portList(const QString& output, const QString& tag)
{
  for(const auto& line : output.split('\n'))
    if(const auto i = line.indexOf(tag); i >= 0)
      return line.mid(i + tag.size()).trimmed().split('|');
  return {};
}
}

TEST_CASE("Structured Light Calibration is created with its ports", "[integration][threedim][structuredlight]")
{
  if(!app::binary_available())
    SKIP("the score application binary was not built");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  auto r = app::run_script(app::write_file(dir, "sl.js", R"JS(
var root = Score.rootInterval();
function ports(name, tag) {
  var p = Score.createProcess(root, name, "");
  if(!p) { console.log("NO-PROCESS " + name); return null; }
  var ins = [], outs = [];
  for (var i = 0; i < Score.inlets(p); i++) ins.push(Score.portName(Score.inlet(p, i)));
  for (var i = 0; i < Score.outlets(p); i++) outs.push(Score.portName(Score.outlet(p, i)));
  console.log("[" + tag + "-IN] " + ins.join("|"));
  console.log("[" + tag + "-OUT] " + outs.join("|"));
  return p;
}
var p = ports("Structured Light Calibration", "SL");
ports("Projector Blend Maps", "BLEND");
if(p) {
  Score.setValue(Score.inlet(p, "Settle (ms)"), 250);
  Score.setValue(Score.inlet(p, "Pattern level"), 0.5);
  console.log("[SETTLE] " + Score.inlet(p, "Settle (ms)").value);
}
Qt.exit(0);
)JS"));
  CHECK_FALSE(r.crashed);
  CHECK(r.exit_code == 0);
  INFO(r.output.toStdString());
  REQUIRE_FALSE(r.output.contains("NO-PROCESS"));

  const auto ins = portList(r.output, "[SL-IN] ");
  const auto outs = portList(r.output, "[SL-OUT] ");
  for(const char* in :
      {"Camera", "Positions", "World from camera", "Projector size", "Mode", "Camera FOV",
       "Camera intrinsics", "Pattern level", "Min stripe px", "Settle (ms)",
       "Frames to average", "Contrast threshold", "Start", "Abort", "Save file", "Save",
       "Load file", "Surface", "Max planes", "Plane threshold (m)", "Phase shift",
       "Phase steps", "Phase period px", "Projector gamma", "Phase min modulation",
       "Intrinsics mode", "Locked intrinsics", "Throw ratio", "Lens shift",
       "Accumulate intrinsics", "Projector count", "Timeout (ms)", "Auto exposure after",
       "Response levels", "Surface compensation", "Exposures", "Capture exposure",
       "Exposure settle (ms)", "Colour mixing levels", "Auto save"})
  {
    INFO("inlet " << in);
    CHECK(ins.contains(QString::fromLatin1(in)));
  }
  for(const char* out :
      {"Pattern", "XYZ map", "Decoded", "Intrinsics", "Pose m0", "Pose m3",
       "World from projector", "RMS", "Coverage", "Progress", "Status", "Done", "Planes",
       "Intrinsics (accumulated)", "Pattern 2", "Pattern 4", "Camera exposure",
       "Auto exposure"})
  {
    INFO("outlet " << out);
    CHECK(outs.contains(QString::fromLatin1(out)));
  }

  const auto blendIns = portList(r.output, "[BLEND-IN] ");
  const auto blendOuts = portList(r.output, "[BLEND-OUT] ");
  for(const char* in : {"Calibration files", "Compute"})
  {
    INFO("blend inlet " << in);
    CHECK(blendIns.contains(QString::fromLatin1(in)));
  }
  CHECK(blendOuts.contains(QStringLiteral("Status")));
  for(const char* gone : {"Canvas layout", "Canvas size"})
    CHECK_FALSE(ins.contains(QString::fromLatin1(gone)));
  CHECK_FALSE(outs.contains(QStringLiteral("Canvas")));
  CHECK(r.output.contains("[SETTLE] 250"));
}
