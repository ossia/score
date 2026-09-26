// score's host side of the Bitfocus companion module protocol, against a mock
// module (data/bitfocus-mock) which records what it receives. The expected
// values are those companion's own host (ChildHandlerLegacy) produces.
#include <Protocols/Bitfocus/BitfocusContext.hpp>
#include <Protocols/Bitfocus/BitfocusProtocol.hpp>

#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/base/node_functions.hpp>
#include <ossia/network/generic/generic_device.hpp>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QNetworkDatagram>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUdpSocket>

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <thread>

namespace bitfocus
{
QString nodeExecutable(const QString&)
{
  return QStandardPaths::findExecutable("node");
}
}

namespace
{
QCoreApplication& app()
{
  static int argc = 1;
  static char arg0[] = "test";
  static char* argv[] = {arg0, nullptr};
  static QCoreApplication a{argc, argv};
  return a;
}

bool waitFor(const std::function<bool()>& pred, int ms = 10000)
{
  QElapsedTimer t;
  t.start();
  while(!pred() && t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
  return pred();
}

struct mock
{
  QTemporaryDir dir;
  QString logPath = dir.path() + "/log.jsonl";
  std::shared_ptr<bitfocus::module_handler> handler;
  bool registered{};

  explicit mock(
      bitfocus::module_configuration conf = {}, bool firstInit = false,
      std::optional<int> upgradeIndex = 3,
      std::optional<std::set<QString>> secretKeys = std::nullopt)
  {
    app();
    qputenv("MOCK_LOG", logPath.toUtf8());
    handler = std::make_shared<bitfocus::module_handler>(
        SCORE_BITFOCUS_MOCK_DIR, "main.js", "node22", "1.14.1", std::move(conf), "mock",
        firstInit, upgradeIndex, std::move(secretKeys));
    handler->afterRegistration([this] { registered = true; });
  }

  std::vector<QJsonObject> events(const QString& ev) const
  {
    std::vector<QJsonObject> res;
    QFile f{logPath};
    if(!f.open(QIODevice::ReadOnly))
      return res;
    for(auto& line : f.readAll().split('\n'))
    {
      auto obj = QJsonDocument::fromJson(line).object();
      if(obj["ev"].toString() == ev)
        res.push_back(obj);
    }
    return res;
  }

  ~mock()
  {
    handler.reset();
    qunsetenv("MOCK_LOG");
  }

  const QJsonObject waitEvent(const QString& ev, std::size_t count = 1)
  {
    waitFor([&] { return events(ev).size() >= count; });
    auto e = events(ev);
    return e.size() >= count ? e[count - 1] : QJsonObject{};
  }
};

struct device
{
  mock& m;
  std::shared_ptr<ossia::net::network_context> ctx
      = std::make_shared<ossia::net::network_context>();
  std::unique_ptr<ossia::net::generic_device> dev;

  explicit device(mock& m)
      : m{m}
  {
    auto proto = std::make_unique<ossia::net::bitfocus_protocol>(m.handler, ctx);
    auto pproto = proto.get();
    dev = std::make_unique<ossia::net::generic_device>(std::move(proto), "mock");
    m.handler->afterRegistration([pproto] { pproto->init_device(); });
    REQUIRE(waitFor([&] { return m.registered; }));
  }

  ossia::net::parameter_base* param(std::string_view path)
  {
    auto n = ossia::net::find_node(dev->get_root_node(), path);
    return n ? n->get_parameter() : nullptr;
  }

  const QJsonObject run(std::string_view path, ossia::value v = ossia::impulse{})
  {
    auto count = m.events("executeAction").size();
    auto p = param(path);
    REQUIRE(p);
    p->push_value(std::move(v));
    return m.waitEvent("executeAction", count + 1)["msg"]["action"].toObject();
  }
};

// Set SCORE_TESTS_REQUIRE_NODE where node is expected, so that it is not skipped
bool hasNode()
{
  if(!QStandardPaths::findExecutable("node").isEmpty())
    return true;
  if(qEnvironmentVariableIsSet("SCORE_TESTS_REQUIRE_NODE"))
    FAIL("node is not installed");
  SKIP("node is not installed");
  return false;
}
}

TEST_CASE("init carries the saved configuration", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m{
      {{"host", "10.0.0.1"}, {"port", 99}, {"password", "pw"}},
      false,
      2,
      std::set<QString>{"password"}};
  REQUIRE(waitFor([&] { return m.registered; }));

  const auto init = m.waitEvent("init")["msg"].toObject();
  CHECK(init["isFirstInit"] == false);
  CHECK(init["label"] == "mock");
  CHECK(init["lastUpgradeIndex"] == 2);
  CHECK(init["config"]["host"] == "10.0.0.1");
  CHECK(init["config"]["port"] == 99);
  CHECK(init["secrets"]["password"] == "pw");
  // Known from the document: split from the first message
  CHECK(!init["config"].toObject().contains("password"));
  CHECK(!init["secrets"].toObject().contains("host"));

  CHECK(m.handler->model().upgradeIndex == 3);

  // Each connection has its own id, as in companion
  mock other;
  REQUIRE(waitFor([&] { return other.registered; }));
  const auto id1 = m.waitEvent("init")["connectionId"].toString();
  const auto id2 = other.waitEvent("init")["connectionId"].toString();
  CHECK(!id1.isEmpty());
  CHECK(id1 != id2);
}

TEST_CASE("a configuration of unknown age goes through every upgrade script", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m{{{"host", "a"}}, false, std::nullopt};
  REQUIRE(waitFor([&] { return m.registered; }));
  CHECK(m.waitEvent("init")["msg"]["lastUpgradeIndex"] == -1);
}

TEST_CASE("a new connection gets the configuration the module computes", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m{{}, true, std::nullopt};
  bool saved = false;
  QObject::connect(
      m.handler.get(), &bitfocus::module_handler::configurationSaved,
      [&] { saved = true; });
  REQUIRE(waitFor([&] { return m.registered; }));

  const auto init = m.waitEvent("init")["msg"].toObject();
  CHECK(init["isFirstInit"] == true);
  CHECK(init["lastUpgradeIndex"] == -1);
  CHECK(saved);
  CHECK(m.handler->model().config.at("port").toInt() == 1234);
}

TEST_CASE("the configuration fields do not wait for init", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m{{{"hang", true}}};
  CHECK(waitFor([&] { return !m.handler->model().config_fields.empty(); }));
  CHECK(!m.registered);
}

TEST_CASE("configuration updates split secrets and skip no-ops", "[bitfocus]")
{
  if(!hasNode())
    return;
  bitfocus::module_configuration conf{{"host", "a"}, {"port", 1}, {"password", "pw"}};
  mock m{conf};
  REQUIRE(waitFor([&] { return m.registered; }));

  m.handler->updateConfigAndLabel("mock", m.handler->model().config);
  conf["host"] = "b";
  m.handler->updateConfigAndLabel("mock", conf);
  const auto upd = m.waitEvent("updateConfigAndLabel")["msg"].toObject();
  CHECK(m.events("updateConfigAndLabel").size() == 1);
  CHECK(upd["config"]["host"] == "b");
  CHECK(!upd["config"].toObject().contains("password"));
  CHECK(upd["secrets"]["password"] == "pw");
}

TEST_CASE("actions are sent with the types the module declared", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};

  CHECK(!d.param("/action/typed/info"));
  REQUIRE(d.param("/action/typed/int"));
  CHECK(d.param("/action/typed/int")->get_value_type() == ossia::val_type::INT);
  CHECK(d.param("/action/typed/float")->get_value_type() == ossia::val_type::FLOAT);
  CHECK(d.param("/action/typed/multi")->get_value_type() == ossia::val_type::LIST);
  CHECK(d.param("/action/typed/check")->get_value_type() == ossia::val_type::BOOL);
  CHECK(d.param("/action/typed/color")->get_value_type() == ossia::val_type::INT);

  const auto a = d.run("/action/typed");
  CHECK(a["actionId"] == "typed");
  CHECK(a["controlId"] == "action/typed");
  const auto opts = a["options"].toObject();
  CHECK(opts["mixed"] == "1");
  CHECK(opts["numeric"] == 2);
  CHECK(opts["multi"] == QJsonValue(QJsonArray{}));
  CHECK(opts["int"] == 3);
  CHECK(opts["float"] == 0.5);
  CHECK(opts["check"] == true);
  CHECK(opts["color"] == 0xff0000);
  CHECK(opts["text"] == "hello");
  CHECK(!opts.contains("nodefault"));
  CHECK(opts["tenth"] == 0.1);
  CHECK(!opts.contains("numnodefault"));
  CHECK(opts["boolnum"] == 0);
  CHECK(opts["numstring"] == "");
  CHECK(opts["multiscalar"] == "a");
  CHECK(!opts.contains("info"));

  d.param("/action/typed/numeric")->push_value(std::string("1"));
  d.param("/action/typed/multi")->push_value(std::vector<ossia::value>{std::string("b")});
  const auto opts2 = d.run("/action/typed")["options"].toObject();
  CHECK(opts2["numeric"] == 1);
  CHECK(opts2["multi"] == QJsonValue(QJsonArray{"b"}));

  // Set explicitly, a value equal to the placeholder is sent all the same
  d.param("/action/typed/numnodefault")->push_value(0);
  d.param("/action/typed/text")->push_value(std::string("hello"));
  const auto opts3 = d.run("/action/typed")["options"].toObject();
  CHECK(opts3["numnodefault"] == 0);
  CHECK(opts3["text"] == "hello");

  // An action with a single option runs when that option is set
  const auto single = d.run("/action/single/choice", std::string("2"));
  CHECK(single["actionId"] == "single");
  CHECK(single["options"]["choice"] == 2);
}

TEST_CASE("feedbacks are subscribed and deliver typed values", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};

  const auto sub = m.waitEvent("updateFeedbacks")["msg"]["feedbacks"].toObject();
  REQUIRE(sub.contains("state"));
  const auto state = sub["state"].toObject();
  CHECK(state["feedbackId"] == "state");
  // Distinct controls, as distinct buttons in companion
  CHECK(state["controlId"] == "feedback/state");
  CHECK(sub["level"]["controlId"] == "feedback/level");
  CHECK(state["options"]["which"] == "on");
  CHECK(state["isInverted"] == false);
  CHECK(state["image"]["width"] == 72);
  CHECK(state["image"]["height"] == 58);

  auto p = d.param("/feedback/state");
  REQUIRE(p);
  CHECK(waitFor([&] { return p->value() == ossia::value{true}; }));
  auto level = d.param("/feedback/level");
  REQUIRE(level);
  CHECK(waitFor([&] { return level->value() == ossia::value{0.25f}; }));

  // Changing an option re-subscribes the feedback with it
  d.param("/feedback/state/which")->push_value(std::string("off"));
  const auto resub = m.waitEvent("updateFeedbacks", 2)["msg"]["feedbacks"].toObject();
  CHECK(resub["state"]["options"]["which"] == "off");
  CHECK(waitFor([&] { return p->value() == ossia::value{false}; }));
}

TEST_CASE("variables and later definitions reach the tree", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};

  auto name = d.param("/variable/name");
  auto count = d.param("/variable/count");
  REQUIRE(name);
  REQUIRE(count);
  CHECK(name->value() == ossia::value{std::string("mock")});
  CHECK(count->value() == ossia::value{42});

  CHECK(!d.param("/action/added"));
  d.run("/action/redefine");
  CHECK(waitFor([&] { return d.param("/action/added") != nullptr; }));
  CHECK(waitFor([&] { return count->value() == ossia::value{43}; }));
  CHECK(d.param("/action/typed/int"));
}

TEST_CASE("actions run from another thread while definitions change", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};

  // The execution engine pushes from its own thread
  auto typed = d.param("/action/typed");
  REQUIRE(typed);
  d.run("/action/churn");
  std::atomic_bool stop{};
  std::thread engine;
  struct join_on_exit
  {
    std::atomic_bool& stop;
    std::thread& t;
    ~join_on_exit()
    {
      stop = true;
      if(t.joinable())
        t.join();
    }
  } guard{stop, engine};
  engine = std::thread{[&] {
    while(!stop)
    {
      typed->push_value(ossia::impulse{});
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }};
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < 2000)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  stop = true;
  engine.join();

  CHECK(waitFor([&] { return m.events("executeAction").size() > 10; }));
}

TEST_CASE("definitions sent again unchanged leave the tree alone", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};
  auto name = d.param("/variable/name");
  REQUIRE(name);

  int created = 0, removed = 0;
  auto& dev = *d.dev;
  struct counter
  {
    int& n;
    void operator()(const ossia::net::node_base&) { n++; }
  };
  counter c1{created}, c2{removed};
  dev.on_node_created.connect(c1);
  dev.on_node_removing.connect(c2);

  d.param("/action/many/count")->set_value(10);
  d.run("/action/many");
  REQUIRE(waitFor([&] { return name->value() == ossia::value{std::string("many:10")}; }));
  CHECK(created == 10);
  CHECK(removed == 0);

  created = 0;
  d.run("/action/many");
  REQUIRE(waitFor([&] { return m.events("executeAction").size() >= 2; }));
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < 300)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
  CHECK(created == 0);
  CHECK(removed == 0);

  dev.on_node_created.disconnect(c1);
  dev.on_node_removing.disconnect(c2);
}

TEST_CASE("numbers do not change the type of a parameter back and forth", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};
  auto count = d.param("/variable/count");
  auto name = d.param("/variable/name");
  REQUIRE(count);
  REQUIRE(name);

  int typeChanges = 0;
  struct counter
  {
    int& n;
    const ossia::net::node_base* node;
    void operator()(const ossia::net::node_base& n_, ossia::string_view attr)
    {
      if(&n_ == node && attr == ossia::net::text_value_type())
        n++;
    }
  } c{typeChanges, &count->get_node()};
  d.dev->on_attribute_modified.connect(c);

  d.run("/action/levels");
  REQUIRE(waitFor([&] { return count->value() == ossia::value{0.25f}; }));
  d.dev->on_attribute_modified.disconnect(c);

  // 42 -> 0.5 upgrades to float once; 1 and 2 then stay floats
  CHECK(typeChanges == 1);
  CHECK(count->get_value_type() == ossia::val_type::FLOAT);

  // Beyond 32 bits an integer stays exact
  CHECK(waitFor([&] { return name->value() == ossia::value{std::string("1790000000123")}; }));
}

TEST_CASE("only new or changed feedbacks are subscribed again", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};
  REQUIRE(!m.waitEvent("updateFeedbacks").isEmpty());
  const auto before = m.events("updateFeedbacks").size();

  d.run("/action/addFeedback");
  const auto upd = m.waitEvent("updateFeedbacks", before + 1)["msg"]["feedbacks"].toObject();
  CHECK(upd.keys() == QStringList{"extra"});
}

TEST_CASE("a value for an undeclared variable creates it", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};
  d.run("/action/undeclared");
  CHECK(waitFor([&] {
    auto p = d.param("/variable/extra");
    return p && p->value() == ossia::value{std::string("hello")};
  }));
}

TEST_CASE("a message of several megabytes", "[bitfocus][.bench]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};
  auto name = d.param("/variable/name");
  REQUIRE(name);
  QElapsedTimer t;
  t.start();
  d.run("/action/huge");
  REQUIRE(waitFor([&] { return name->value() == ossia::value{std::string("after huge")}; }, 60000));
  std::printf("5 MB message: %lld ms\n", (long long)t.elapsed());
  CHECK(m.handler->model().presets.size() == 20000);
}

TEST_CASE("tree sync with thousands of variables", "[bitfocus][.bench]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};
  auto name = d.param("/variable/name");
  REQUIRE(name);
  auto time = [&](const char* what, int count) {
    QElapsedTimer t;
    t.start();
    const auto expected = ossia::value{"many:" + std::to_string(count)};
    name->set_value(std::string{});
    d.param("/action/many/count")->set_value(count);
    d.run("/action/many");
    REQUIRE(waitFor([&] { return name->value() == expected; }, 60000));
    // Queued after the tree update: done once the module has run it
    d.run("/action/typed");
    std::printf("%s: %lld ms\n", what, (long long)t.elapsed());
  };
  time("6000 variables, first time", 6000);
  time("6000 variables, unchanged", 6000);
  time("6001 variables", 6001);
}

TEST_CASE("failed actions are reported and do not stop later ones", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};
  d.run("/action/fail");
  CHECK(d.run("/action/typed")["actionId"] == "typed");
}

TEST_CASE("shared UDP sockets carry binary data both ways", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};

  QUdpSocket probe;
  REQUIRE(probe.bind(QHostAddress::LocalHost, 0));
  const int port = probe.localPort();
  probe.close();

  d.param("/action/udp/port")->push_value(port);
  const auto joined = m.waitEvent("udpJoined");
  REQUIRE(!joined["handle"].toString().isEmpty());

  QUdpSocket peer;
  REQUIRE(peer.bind(QHostAddress::LocalHost, 0));
  const QByteArray payload("\x01\x02hi", 4);
  peer.writeDatagram(payload, QHostAddress::LocalHost, port);

  const auto msg = m.waitEvent("udpMessage");
  CHECK(msg["isBuffer"] == true);
  CHECK(msg["data"] == QString::fromLatin1(payload.toHex()));
  CHECK(msg["sameHandle"] == true);
  CHECK(msg["source"]["family"] == "IPv4");
  CHECK(msg["source"]["address"] == "127.0.0.1");
  CHECK(msg["source"]["port"] == peer.localPort());

  REQUIRE(waitFor([&] { return peer.hasPendingDatagrams(); }));
  CHECK(peer.receiveDatagram().data() == "echo:" + payload);
}

TEST_CASE("send-osc encodes typed arguments", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m;
  device d{m};

  QUdpSocket recv;
  REQUIRE(recv.bind(QHostAddress::LocalHost, 0));
  // A host name, resolved without blocking
  d.param("/action/osc/host")->push_value(std::string("localhost"));
  d.param("/action/osc/port")->push_value((int)recv.localPort());
  d.run("/action/osc");

  REQUIRE(waitFor([&] { return recv.hasPendingDatagrams(); }));
  const auto data = recv.receiveDatagram().data();
  const QByteArray expected
      = QByteArray("/mock\0\0\0,isb\0\0\0\0", 16) + QByteArray("\0\0\0\x07", 4)
        + QByteArray("x\0\0\0", 4) + QByteArray("\0\0\0\x03\x01\x02\x03\0", 8);
  CHECK(data == expected);
}

TEST_CASE("a module which crashes is started again", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m{{{"host", "a"}}};
  device d{m};
  REQUIRE(m.waitEvent("updateFeedbacks").contains("msg"));

  d.run("/action/crash");
  // Restarted with the configuration it had, not as a new connection
  const auto init = m.waitEvent("init", 2)["msg"].toObject();
  REQUIRE(!init.isEmpty());
  CHECK(init["isFirstInit"] == false);
  CHECK(init["config"]["host"] == "a");

  // Feedbacks subscribed again, actions running again
  CHECK(!m.waitEvent("updateFeedbacks", 2).isEmpty());
  CHECK(waitFor([&] { return m.handler->model().actions.size() > 0 && d.param("/action/typed"); }));
  CHECK(d.run("/action/typed")["actionId"] == "typed");
}

TEST_CASE("the module is told to destroy itself before it is stopped", "[bitfocus]")
{
  if(!hasNode())
    return;
  mock m{{{"slowDestroy", true}}};
  REQUIRE(waitFor([&] { return m.registered; }));

  // Without waiting for the module to be done
  QElapsedTimer t;
  t.start();
  m.handler.reset();
  CHECK(t.elapsed() < 200);
  CHECK(waitFor([&] { return m.events("destroy").size() == 1; }));
}
