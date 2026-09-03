// Qt.vector3d() and its siblings in a headless `score --script` run.
//
// Qt.vector3d(1, 2, 3) used to evaluate to (0, 0, 0) there. Not an error, not
// a warning: a zero vector. Qt builds these through
// QQmlValueTypeProvider::createValueType(), which needs a QML value type
// registered against the target metatype; when there is none it returns an
// invalid QVariant and qqmlbuiltinfunctions.cpp answers
//
//     return variant.isValid() ? variant : QVariant(type);
//
// -- a default-constructed value, silently. The five affected constructors
// (vector2d, vector3d, vector4d, quaternion, matrix4x4) get their value types
// from QtQuick, whose types are registered on the first `import QtQuick`. A JS
// process performs one -- the default script in JSProcessModel.cpp opens with
// it -- so the call worked there; a --script run, the console panel and a .mjs
// module import nothing at all, and got zeros.
//
// Two halves, and the second is the control. "headless" is the defect: it is
// red on the parent commit and green after it. "with QtQuick imported" runs
// the SAME script in a process where a QML file has imported QtQuick before
// the script runs -- the state a JS process leaves behind. It was already
// green and it has to stay green: if it went red the fix would have broken the
// path that worked, and if it had been red to begin with the diagnosis (a
// missing import, not a missing implementation) would have been wrong.
//
// Not covered by the value types: Qt.rect(), Qt.point() and Qt.size() return
// QRectF/QPointF/QSizeF directly and QtQml registers those itself, so they
// were never affected -- they are asserted below to keep that distinction
// honest. Qt.font() and the array form of Qt.matrix4x4() do check the result
// and throw, so they failed loudly rather than silently.

#include <QByteArray>
#include <QFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

namespace
{
QString appBinary()
{
#if defined(SCORE_APP_BINARY)
  return QStringLiteral(SCORE_APP_BINARY);
#else
  return {};
#endif
}

struct Run
{
  int exitCode{-1};
  bool crashed{true};
  QString output;
};

Run runScript(const QStringList& extraArgs, const QString& path)
{
  auto env = QProcessEnvironment::systemEnvironment();
  env.remove("DISPLAY");
  env.remove("WAYLAND_DISPLAY");
  env.insert("QT_QPA_PLATFORM", "offscreen");
  env.insert("SCORE_AUDIO_BACKEND", "dummy");
  env.insert("SCORE_DISABLE_AUDIOPLUGINS", "1");
  // Every value this test checks is reported through console.log, i.e. a
  // qDebug, and the child's stderr is a pipe: where Qt is built against
  // journald it logs there instead and the checks all read an empty string.
  env.insert("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
  env.insert("QT_FORCE_STDERR_LOGGING", "1");

  QProcess p;
  p.setProcessEnvironment(env);
  p.setProcessChannelMode(QProcess::MergedChannels);
  p.start(
      appBinary(),
      QStringList{"--no-gui", "--no-restore", "--wait", "0"} + extraArgs
          + QStringList{"--script", path});

  Run r;
  if(!p.waitForStarted(30000) || !p.waitForFinished(120000))
  {
    p.kill();
    p.waitForFinished(5000);
    return r;
  }
  r.output = QString::fromUtf8(p.readAll());
  r.crashed = p.exitStatus() != QProcess::NormalExit;
  r.exitCode = p.exitCode();
  return r;
}

QString write(const QTemporaryDir& dir, const QString& name, const QByteArray& body)
{
  const QString path = dir.filePath(name);
  QFile f{path};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(body);
  f.close();
  return path;
}

// Every line reports through `attempt`, so one unregistered value type cannot
// end the script and hide the state of the others: without the registration
// the sub-properties read back undefined and the methods are not callable at
// all, and either way the run still reaches Qt.exit(0) and prints a full
// picture of what worked.
//
// v3str is the reported symptom itself, printed the way a script would print
// it: it read "QVariant(QVector3D, QVector3D(0, 0, 0))" for Qt.vector3d(1,2,3).
const char* script = R"JS(
function say(k, v) { console.log(k + "=" + v); }
function attempt(k, f) { try { say(k, f()); } catch(e) { say(k, "threw"); } }

attempt("v3str", function() { return "" + Qt.vector3d(1, 2, 3); });

attempt("v2", function() { var v = Qt.vector2d(1, 2); return v.x + "," + v.y; });
attempt("v3", function() {
  var v = Qt.vector3d(1, 2, 3); return v.x + "," + v.y + "," + v.z; });
attempt("v3len", function() { return Qt.vector3d(3, 4, 0).length(); });
attempt("v3dot", function() {
  return Qt.vector3d(1, 2, 3).dotProduct(Qt.vector3d(4, 5, 6)); });
attempt("v4", function() {
  var v = Qt.vector4d(1, 2, 3, 4);
  return v.x + "," + v.y + "," + v.z + "," + v.w; });
attempt("q", function() {
  var v = Qt.quaternion(1, 2, 3, 4);
  return v.scalar + "," + v.x + "," + v.y + "," + v.z; });
attempt("m", function() {
  var m = Qt.matrix4x4(1, 0, 0, 5,  0, 1, 0, 6,  0, 0, 1, 7,  0, 0, 0, 1);
  return m.m14 + "," + m.m24 + "," + m.m34; });
attempt("mid", function() {
  return Qt.matrix4x4().m11 + "," + Qt.matrix4x4().m22; });
// QColor: Qt.rgba() itself always worked -- its colour provider is installed
// from a load-time constructor in QtQuick rather than on import -- but reading
// the channels back off the result needs the same value type as the vectors.
attempt("rgba", function() {
  var c = Qt.rgba(1, 0, 0, 1); return c.r + "," + c.g + "," + c.b + "," + c.a; });

// Registered by QtQml itself, so these worked all along.
attempt("rect", function() {
  var r = Qt.rect(1, 2, 3, 4);
  return r.x + "," + r.y + "," + r.width + "," + r.height; });
attempt("point", function() { var p = Qt.point(8, 9); return p.x + "," + p.y; });
attempt("size", function() {
  var s = Qt.size(10, 11); return s.width + "," + s.height; });

Qt.exit(0);
)JS";

void checkAllConstructed(const Run& r)
{
  INFO(r.output.toStdString());
  CHECK_FALSE(r.crashed);
  CHECK(r.exitCode == 0);

  CHECK(r.output.contains("v3str=QVector3D(1, 2, 3)"));

  CHECK(r.output.contains("v2=1,2"));
  CHECK(r.output.contains("v3=1,2,3"));
  CHECK(r.output.contains("v3len=5"));
  CHECK(r.output.contains("v3dot=32"));
  CHECK(r.output.contains("v4=1,2,3,4"));
  CHECK(r.output.contains("q=1,2,3,4"));
  CHECK(r.output.contains("m=5,6,7"));
  CHECK(r.output.contains("mid=1,1"));
  CHECK(r.output.contains("rgba=1,0,0,1"));

  CHECK(r.output.contains("rect=1,2,3,4"));
  CHECK(r.output.contains("point=8,9"));
  CHECK(r.output.contains("size=10,11"));
}
}

TEST_CASE("Qt.vector3d and its siblings build real values", "[integration][js][valuetypes]")
{
  if(appBinary().isEmpty() || !QFile::exists(appBinary()))
    SKIP("the score application binary was not built");

  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const QString js = write(dir, "valuetypes.js", QByteArray(script));

  SECTION("headless, nothing imported")
  {
    checkAllConstructed(runScript({}, js));
  }

  SECTION("with QtQuick imported, as in the GUI")
  {
    // --ui loads this into the console engine before the script runs. The root
    // is a QtObject rather than an Item on purpose: ApplicationPlugin only
    // opens a QQuickWindow for a QQuickItem, so the import happens and no
    // window, scene graph or GPU context is needed for it.
    const QString qml = write(
        dir, "quick.qml", "import QtQuick\nimport QtQml\nQtObject { }\n");
    checkAllConstructed(runScript({"--ui", qml}, js));
  }
}
