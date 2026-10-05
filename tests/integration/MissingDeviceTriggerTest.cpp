// A trigger or condition on an address whose device is absent or not
// connected does not hold. Each case runs the application with --script, which
// reads back sink:/fired, written by the state the trigger ends on.

#include <QByteArray>
#include <QTemporaryDir>
#include <QUdpSocket>

#include <catch2/catch_test_macros.hpp>
#include <score_test/AppProcess.hpp>

namespace
{
namespace app = score::test::app;

int freePort()
{
  QUdpSocket s;
  REQUIRE(s.bind(QHostAddress::LocalHost, 0));
  return s.localPort();
}

//! How the address `%trig:/x%` of the expression is backed.
enum class Device
{
  Absent,
  Disconnected,
  Connected
};

//! Where the expression goes, and what else the script does.
enum class Use
{
  Trigger,
  TriggerWithMax,
  TriggerByHand,
  Condition
};

QString script(Device device, Use use)
{
  const int sinkIn = freePort();
  const int sinkOut = freePort();
  const int trigIn = freePort();
  const int trigOut = freePort();

  QString s = QStringLiteral(R"JS(
var FL = 705600000;
Score.createOSCDevice("sink", "127.0.0.1", %1, %2);
Score.createAddress("sink:/fired", "int");
)JS")
                  .arg(sinkOut)
                  .arg(sinkIn);

  switch(device)
  {
    case Device::Absent:
      break;
    case Device::Disconnected:
      // On the input port sink already holds: it cannot connect.
      s += QStringLiteral(R"JS(
Score.createOSCDevice("trig", "127.0.0.1", %1, %2);
Score.createAddress("trig:/x", "float");
)JS")
               .arg(trigOut)
               .arg(sinkIn);
      break;
    case Device::Connected:
      s += QStringLiteral(R"JS(
Score.createOSCDevice("trig", "127.0.0.1", %1, %2);
Score.createAddress("trig:/x", "float");
)JS")
               .arg(trigOut)
               .arg(trigIn);
      break;
  }

  s += QStringLiteral(R"JS(
var root = Score.rootInterval();
Score.setIntervalDuration(root, 30 * FL);
var scen = Score.process(root, 0);
var lead = Score.createIntervalAfter(Score.startState(scen), 1 * FL, 0.3);
var end = Score.endState(lead);
Score.setMessages(end, [{ "address": "sink:/fired", "value": 1 }]);
Score.createIntervalAfter(end, 1 * FL, 0.3);
)JS");

  switch(use)
  {
    case Use::Trigger:
    case Use::TriggerWithMax:
    case Use::TriggerByHand:
      s += QStringLiteral(R"JS(
Score.enableTrigger(lead);
Score.setExpression(lead, "{ %trig:/x% > 0.5 }");
)JS");
      break;
    case Use::Condition:
      s += QStringLiteral(R"JS(
Score.enableCondition(end);
Score.setExpression(end, "{ %trig:/x% > 0.5 }");
)JS");
      break;
  }
  if(use == Use::TriggerWithMax)
    s += QStringLiteral("Score.setIntervalMaxDuration(lead, 2 * FL);\n");

  s += QStringLiteral(R"JS(
function after(ms, f) {
  var t = Qt.createQmlObject('import QtQml; Timer { repeat: false }', Score, "t");
  t.interval = ms;
  t.triggered.connect(f);
  t.running = true;
}
Score.play();
)JS");
  if(use == Use::TriggerByHand)
    s += QStringLiteral("after(1500, function() { Score.trigger(lead); });\n");
  s += QStringLiteral(R"JS(
after(2700, function() {
  console.log("FIRED=" + Device.read("sink:/fired"));
  Score.stop();
  Qt.exit(0);
});
)JS");
  return s;
}

QString fired(Device device, Use use)
{
  QTemporaryDir dir;
  REQUIRE(dir.isValid());
  const auto path = app::write_file(dir, "trigger.js", script(device, use).toUtf8());
  const auto r = app::run_script(path);
  INFO(r.output.toStdString());
  REQUIRE(r.finished);
  const auto i = r.output.indexOf("FIRED=");
  REQUIRE(i >= 0);
  return r.output.mid(i + 6, 1);
}
}

TEST_CASE("a trigger on an absent device does not fire", "[execution][trigger]")
{
  if(!app::binary_available())
    SKIP("no application binary");
  CHECK(fired(Device::Absent, Use::Trigger) == "0");
}

TEST_CASE("a trigger on a disconnected device does not fire", "[execution][trigger]")
{
  if(!app::binary_available())
    SKIP("no application binary");
  CHECK(fired(Device::Disconnected, Use::Trigger) == "0");
}

TEST_CASE("a trigger on a connected device waits for its value", "[execution][trigger]")
{
  if(!app::binary_available())
    SKIP("no application binary");
  CHECK(fired(Device::Connected, Use::Trigger) == "0");
}

TEST_CASE(
    "a trigger on an absent device still fires at its maximum", "[execution][trigger]")
{
  if(!app::binary_available())
    SKIP("no application binary");
  CHECK(fired(Device::Absent, Use::TriggerWithMax) == "1");
}

TEST_CASE("a trigger on an absent device still fires by hand", "[execution][trigger]")
{
  if(!app::binary_available())
    SKIP("no application binary");
  CHECK(fired(Device::Absent, Use::TriggerByHand) == "1");
}

TEST_CASE("a condition on an absent device does not hold", "[execution][condition]")
{
  if(!app::binary_available())
    SKIP("no application binary");
  CHECK(fired(Device::Absent, Use::Condition) == "0");
}

TEST_CASE("a condition on a disconnected device does not hold", "[execution][condition]")
{
  if(!app::binary_available())
    SKIP("no application binary");
  CHECK(fired(Device::Disconnected, Use::Condition) == "0");
}
