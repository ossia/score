// Driven through a real --script run of the application.
//
// A scenario of three intervals, A then B then C; B and C wait for a trigger
// that never holds. An automation in B writes sink:/b, from 0 at its
// beginning to 1 at its default end, which the script reads back.
//
// - Play from here inside C, then, without stopping, from inside A: B plays
//   when A ends.
// - Play from here inside B, then its stop and play buttons: B plays again
//   from its beginning, not from where play from here put it.

#include <QByteArray>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QUdpSocket>

#include <catch2/catch_test_macros.hpp>
#include <score_test/AppProcess.hpp>

#include <vector>

namespace
{
namespace app = score::test::app;

int freePort()
{
  QUdpSocket s;
  REQUIRE(s.bind(QHostAddress::LocalHost, 0));
  return s.localPort();
}

QString prelude()
{
  return QStringLiteral(R"JS(
var FL = 705600000;
Score.createOSCDevice("sink", "127.0.0.1", %1, %2);
Score.createAddress("sink:/b", "float");
Score.createAddress("sink:/never", "float");
var root = Score.rootInterval();
Score.setIntervalDuration(root, 30 * FL);
var scen = Score.process(root, 0);
var A = Score.createIntervalAfter(Score.startState(scen), 2 * FL, 0.1);
var B = Score.createIntervalAfter(Score.endState(A), 3 * FL, 0.3);
var C = Score.createIntervalAfter(Score.endState(B), 3 * FL, 0.5);
[B, C].forEach(function(itv) {
  Score.setIntervalMinDuration(itv, 0);
  Score.setIntervalMaxInfinite(itv, true);
  Score.enableTrigger(itv);
  Score.setExpression(itv, "{ %sink:/never% > 0.5 }");
});
var au = Score.createProcess(B, "d2a67bd8-5d3f-404e-b6e9-e350cf2a833f", "");
Score.setAddress(Score.outlet(au, 0), "sink:/b");

function after(ms, f) {
  var t = Qt.createQmlObject('import QtQml; Timer { repeat: false }', Score, "t");
  t.interval = ms;
  t.triggered.connect(f);
  t.running = true;
}
function b() { return Number(Device.read("sink:/b")); }
)JS")
      .arg(freePort())
      .arg(freePort());
}

//! The values printed as `B=<value>` by the script, in order.
std::vector<double> run(const QString& steps)
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto path
      = app::write_file(dir, "play-from-here.js", (prelude() + steps).toUtf8());
  const auto r = app::run_script(path);
  INFO(r.output.toStdString());
  REQUIRE(r.finished);

  std::vector<double> values;
  static const QRegularExpression re{QStringLiteral("B=(-?[0-9.e+-]+)")};
  for(auto it = re.globalMatch(r.output); it.hasNext();)
    values.push_back(it.next().captured(1).toDouble());
  return values;
}
}

TEST_CASE(
    "play from here to an earlier date while playing plays what comes after",
    "[execution][transport]")
{
  if(!app::binary_available())
    SKIP("no application binary");

  // At 1 s in A, B starts 1 s later: read it 1 s and 1.5 s into B.
  const auto values = run(QStringLiteral(R"JS(
Score.playFromHere(6000);
after(500, function() { Score.playFromHere(1000); });
after(2500, function() { console.log("B=" + b()); });
after(3000, function() { console.log("B=" + b()); Score.stop(); Qt.exit(0); });
)JS"));
  REQUIRE(values.size() == 2);
  CHECK(values[0] > 0.1);
  CHECK(values[1] > values[0]);
}

TEST_CASE(
    "an interval stopped and played again after play from here starts over",
    "[execution][transport]")
{
  if(!app::binary_available())
    SKIP("no application binary");

  // Play from here 1.5 s into B: halfway. Stopped, then played again, it
  // starts from 0.
  const auto values = run(QStringLiteral(R"JS(
Score.playFromHere(3500);
after(500, function() { console.log("B=" + b()); Score.stop(B); });
after(800, function() { Score.play(B); });
after(1100, function() { console.log("B=" + b()); Score.stop(); Qt.exit(0); });
)JS"));
  REQUIRE(values.size() == 2);
  CHECK(values[0] > 0.5);
  CHECK(values[1] < 0.3);
}
