// Score.setValue and port property writes against the kind of port they land on.
//
// - A number written to a Bool, Int or VecN control keeps the control's type:
//   setValue(toggle, 1) stores a bool, and a JS array written to a vec3
//   control stores a vec3.
// - setValue on a port that is not a control (a plain value inlet, e.g. the
//   list inputs of Scene Graph Filter) cannot store anything; it used to
//   return without a word and now warns.
// - `port.renderSize = Qt.size(w, h)` on a texture inlet threw
//   "Cannot assign QSizeF to std::optional<QSize>".
//
// A subprocess test: what is under test is the effect on a live document of a
// real --script run.

#include <QByteArray>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>
#include <score_test/AppProcess.hpp>

#if !defined(GFX_TEST_CORPUS_DIR)
#define GFX_TEST_CORPUS_DIR ""
#endif

namespace
{
namespace app = score::test::app;
using Run = app::result;

const char* prelude = R"JS(
var root = Score.rootInterval();
function control(procName, portName) {
  var p = Score.createProcess(root, procName, "");
  if(!p) { console.log("NO-PROCESS " + procName); Qt.exit(0); }
  var port = Score.inlet(p, portName);
  if(!port) { console.log("NO-PORT " + portName); Qt.exit(0); }
  return port;
}
function show(label, port) {
  console.log("[T] " + label + " " + Score.valueType(port) + " " + port.value);
}
)JS";

void skipIfMissing(const Run& r)
{
  if(r.output.contains("NO-PROCESS") || r.output.contains("NO-PORT"))
    SKIP("a process this test needs is not available in this build");
}
}

TEST_CASE("Score.setValue keeps a control's value type", "[integration][js][scripting]")
{
  if(!app::binary_available())
    SKIP("the score application binary was not built");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());

  SECTION("an integer written to a Bool control is a bool")
  {
    auto r = app::run_script(app::write_file(
        dir, "bool.js",
        QByteArray(prelude)
            + R"JS(
var t = control("Scene Graph Filter", "Invert");
show("initial", t);
Score.setValue(t, 1); show("one", t);
Score.setValue(t, 0); show("zero", t);
Score.setValue(t, 2.5); show("float", t);
Score.setValue(t, false); show("false", t);
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    CHECK(r.exit_code == 0);
    skipIfMissing(r);
    CHECK(r.output.contains("[T] initial Bool 0"));
    // The toggle reads a bool, not a Float.
    CHECK(r.output.contains("[T] one Bool 1"));
    CHECK(r.output.contains("[T] zero Bool 0"));
    CHECK(r.output.contains("[T] float Bool 1"));
    CHECK(r.output.contains("[T] false Bool 0"));
  }

  SECTION("a number written to an Int control is an int")
  {
    auto r = app::run_script(app::write_file(
        dir, "int.js",
        QByteArray(prelude)
            + R"JS(
var n = control("Arraygen", "Size");
Score.setValue(n, 5); show("five", n);
Score.setValue(n, 3.7); show("frac", n);
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    skipIfMissing(r);
    CHECK(r.output.contains("[T] five Int 5"));
    CHECK(r.output.contains("[T] frac Int 3"));
  }

  SECTION("a JS array written to a vec3 control is a vec3")
  {
    auto r = app::run_script(app::write_file(
        dir, "vec.js",
        QByteArray(prelude)
            + R"JS(
var v = control("Transform 3D", "Position");
Score.setValue(v, [1, 2, 3]); show("array", v);
Score.setValue(v, [1, 2]); show("short", v);
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    skipIfMissing(r);
    // The node reads a vec3 and ignores a Tuple.
    CHECK(r.output.contains("[T] array Vec3f"));
    // A list of the wrong length is not coerced.
    CHECK(r.output.contains("[T] short Tuple"));
  }

  SECTION("setValue on a port that holds no value says so")
  {
    auto r = app::run_script(app::write_file(
        dir, "valueinlet.js",
        QByteArray(prelude)
            + R"JS(
var names = control("Scene Graph Filter", "Names");
Score.setValue(names, ["a", "b"]);
console.log("DONE");
Qt.exit(0);
)JS"));
    CHECK_FALSE(r.crashed);
    skipIfMissing(r);
    CHECK(r.output.contains("DONE"));
    CHECK(r.output.contains("Score.setValue: port \"Names\" is not a control"));
  }
}

TEST_CASE("A texture inlet's render size is writable from a script", "[integration][js][scripting]")
{
  if(!app::binary_available())
    SKIP("the score application binary was not built");

  const QString shader = QStringLiteral(GFX_TEST_CORPUS_DIR "/isf-image-passthrough.fs");
  REQUIRE(QFile::exists(shader));

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString saved = dir.filePath("rendersize.score");

  auto r = app::run_script(app::write_file(
      dir, "rendersize.js",
      QByteArray(prelude) + "var OUT = " + app::js_string(saved).toUtf8()
          + ";\nvar SHADER = " + app::js_string(shader).toUtf8() + ";\n"
          + R"JS(
var isf = Score.createProcess(root, "ISF Shader", SHADER);
if(!isf) { console.log("NO-SHADER"); Qt.exit(0); }
var p = Score.inlet(isf, "inputImage");
if(!p) { console.log("NO-SHADER"); Qt.exit(0); }
try { p.renderSize = Qt.size(320, 240); console.log("RS-SET"); }
catch(e) { console.log("RS-ERR " + e); }
Score.saveAs(OUT);
console.log("SAVED");
Qt.exit(0);
)JS"));
  INFO(r.output.toStdString());
  CHECK_FALSE(r.crashed);
  CHECK(r.exit_code == 0);
  REQUIRE_FALSE(r.output.contains("NO-SHADER"));
  CHECK(r.output.contains("RS-SET"));
  REQUIRE(r.output.contains("SAVED"));

  QFile f{saved};
  REQUIRE(f.open(QIODevice::ReadOnly));
  const QByteArray json = f.readAll();
  CHECK(QRegularExpression{R"("RenderSize"\s*:\s*\[\s*320\s*,\s*240\s*\])"}
            .match(QString::fromUtf8(json))
            .hasMatch());
}
