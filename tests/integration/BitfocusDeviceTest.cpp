// A Bitfocus companion device in a document, as loading a saved file creates
// it: the settings come from JSON, the module (a mock, see
// src/plugins/score-plugin-protocols/Tests/data/bitfocus-mock) registers
// asynchronously, and the explorer must follow the tree the module defines,
// then and later.
#include <score_test/App.hpp>
#include <score_test/Document.hpp>
#include <score_test/Events.hpp>

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/serialization/JSONVisitor.hpp>

#include <core/document/Document.hpp>

#include <ossia/network/base/device.hpp>
#include <ossia/network/base/node_functions.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QLineEdit>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <utility>

namespace
{
constexpr auto bitfocusKey = "303993ed-b39a-4edb-90a6-2a3ae45043c4";

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

// The module runs in a node process that has to start first.
template <typename F>
bool waitFor(F&& pred)
{
  return score::test::wait_until(std::forward<F>(pred), 15000);
}

//! What the mock module logs, one JSON object per line, in a file of its own.
struct MockLog
{
  QTemporaryDir dir;
  QString path = dir.path() + "/mock.jsonl";
  MockLog() { qputenv("MOCK_LOG", path.toUtf8()); }
  ~MockLog() { qunsetenv("MOCK_LOG"); }

  std::vector<QJsonObject> events(const QString& ev) const
  {
    std::vector<QJsonObject> res;
    QFile f{path};
    if(f.open(QIODevice::ReadOnly))
      for(auto& line : f.readAll().split('\n'))
        if(auto o = QJsonDocument::fromJson(line).object(); o["ev"] == ev)
          res.push_back(o);
    return res;
  }
};

Device::ProtocolFactory& bitfocus(const score::GUIApplicationContext& ctx)
{
  auto* fact = ctx.interfaces<Device::ProtocolFactoryList>().get(
      UuidKey<Device::ProtocolFactory>::fromString(QString(bitfocusKey)));
  REQUIRE(fact);
  return *fact;
}

//! The settings of a device of the mock module as a saved document holds them,
//! \p saved being the keys of the protocol's JSON besides the module's own.
Device::DeviceSettings
mockSettings(Device::ProtocolFactory& fact, const QString& name, const QString& saved)
{
  const auto json = QStringLiteral(R"({
    "Path": "%1", "Entrypoint": "main.js", "Identifier": "score-mock",
    "Name": "Mock", "Brand": "ossia", "Product": "", "NodeVersion": "node22",
    "APIVersion": "1.14.1", "Description": "", %2
  })").arg(QStringLiteral(SCORE_BITFOCUS_MOCK_DIR), saved);
  auto json_doc = readJson(json.toUtf8());
  JSONWriter wrt{json_doc};
  Device::DeviceSettings set;
  set.name = name;
  set.protocol = fact.concreteKey();
  set.deviceSpecificSettings = fact.makeProtocolSpecificSettings(wrt.toVariant());
  return set;
}

const Device::Node* child(const Device::Node& n, const QString& name)
{
  for(auto& c : n)
    if(c.displayName() == name)
      return &c;
  return nullptr;
}

const Device::Node* findPath(const Device::Node& root, const QStringList& path)
{
  const Device::Node* cur = &root;
  for(auto& p : path)
    if(!(cur = child(*cur, p)))
      return nullptr;
  return cur;
}

QJsonObject settingsJson(Device::ProtocolFactory& fact, const Device::DeviceSettings& s)
{
  JSONReader r;
  r.stream.StartObject();
  fact.serializeProtocolSpecificSettings(s.deviceSpecificSettings, r.toVariant());
  r.stream.EndObject();
  return QJsonDocument::fromJson(r.toByteArray()).object();
}
}

TEST_CASE("a saved Bitfocus device follows the module's tree", "[integration][bitfocus]")
{
  if(!hasNode())
    return;

  MockLog mock;
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& devplug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* fact = &bitfocus(ctx);

    // As a document saved before the module had anything to save
    auto set = mockSettings(*fact, "mock", R"("Configuration": [])");

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Explorer::Command::LoadDevice{devplug, std::move(set)});

    auto& root = devplug.explorer().rootNode();
    auto* devNode = child(root, "mock");
    REQUIRE(devNode);

    // The module registers after the device is created
    REQUIRE(waitFor([&] { return findPath(*devNode, {"action", "typed", "int"}) != nullptr; }));
    CHECK(findPath(*devNode, {"feedback", "state", "which"}));
    CHECK(waitFor([&] { return findPath(*devNode, {"variable", "count"}) != nullptr; }));
    CHECK(!findPath(*devNode, {"action", "typed", "info"}));

    // What the new connection saved is kept with the document
    auto* device = devplug.list().findDevice("mock");
    REQUIRE(device);
    CHECK(waitFor([&] {
      return settingsJson(*fact, device->settings()).contains("UpgradeIndex");
    }));
    const auto saved = settingsJson(*fact, device->settings());
    CHECK(saved["UpgradeIndex"] == 3);
    CHECK(saved["Configuration"].toArray().size() >= 2);

    // Definitions sent later reach the explorer as well
    auto* ossiaDev = device->getDevice();
    REQUIRE(ossiaDev);
    auto* redefine = ossia::net::find_node(ossiaDev->get_root_node(), "/action/redefine");
    REQUIRE(redefine);
    redefine->get_parameter()->push_value(ossia::impulse{});
    CHECK(waitFor([&] { return findPath(*devNode, {"action", "added"}) != nullptr; }));

    // Editing the device in the settings dialog reconfigures the running module
    std::unique_ptr<Device::ProtocolSettingsWidget> widget{fact->makeSettingsWidget()};
    widget->setSettings(device->settings());
    QLineEdit* hostEdit{};
    REQUIRE(waitFor([&] { return (hostEdit = widget->findChild<QLineEdit*>("host")); }));
    hostEdit->setText("10.1.2.3");
    device->updateSettings(widget->getSettings());

    CHECK(waitFor([&] { return mock.events("updateConfigAndLabel").size() == 1; }));
    const auto upd = mock.events("updateConfigAndLabel");
    if(!upd.empty())
      CHECK(upd[0]["msg"]["config"]["host"] == "10.1.2.3");
    CHECK(mock.events("init").size() == 1);
    CHECK(waitFor([&] { return findPath(*devNode, {"action", "typed", "int"}) != nullptr; }));
    auto* typed = ossia::net::find_node(device->getDevice()->get_root_node(), "/action/typed");
    REQUIRE(typed);
    typed->get_parameter()->push_value(ossia::impulse{});
    CHECK(waitFor([&] { return mock.events("executeAction").size() >= 2; }));
  });
}

TEST_CASE("a loaded Bitfocus device keeps what the module upgraded", "[integration][bitfocus]")
{
  if(!hasNode())
    return;

  MockLog mock;
  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& devplug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* fact = &bitfocus(ctx);

    auto set = mockSettings(*fact, "mock", R"("UpgradeIndex": 2,
      "Configuration": [["host", {"String": "10.0.0.1"}], ["version", {"String": "old"}]])");
    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Explorer::Command::LoadDevice{devplug, std::move(set)});

    auto* device = devplug.list().findDevice("mock");
    REQUIRE(device);
    auto versionOf = [&] {
      for(auto kv : settingsJson(*fact, device->settings())["Configuration"].toArray())
        if(kv.toArray()[0] == "version")
          return QJsonDocument{kv.toArray()[1].toObject()}.toJson(QJsonDocument::Compact);
      return QByteArray{};
    };
    REQUIRE(waitFor([&] { return versionOf() == R"({"String":"new"})"; }));

    // Nothing is sent back over it once the module is initialised
    score::test::run_events_for(500);
    CHECK(mock.events("updateConfigAndLabel").empty());
    REQUIRE(mock.events("init").size() == 1);
    CHECK(std::as_const(mock.events("init")[0])["msg"]["isFirstInit"] == false);
    CHECK(versionOf() == R"({"String":"new"})");

    // A module without configuration was initialised before: not a new connection
    auto set2 = mockSettings(*fact, "mock2", R"("UpgradeIndex": 3, "Configuration": [])");
    disp.submit(new Explorer::Command::LoadDevice{devplug, std::move(set2)});
    REQUIRE(waitFor([&] { return mock.events("init").size() == 2; }));
    CHECK(std::as_const(mock.events("init")[1])["msg"]["isFirstInit"] == false);
  });
}

TEST_CASE("the Bitfocus settings dialog saves what companion would", "[integration][bitfocus]")
{
  if(!hasNode())
    return;

  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* fact = &bitfocus(ctx);

    // A device picked in the list, before anything is configured
    auto set = mockSettings(*fact, "mock", R"("Configuration": [])");

    std::unique_ptr<Device::ProtocolSettingsWidget> widget{fact->makeSettingsWidget()};
    REQUIRE(widget);
    widget->setSettings(set);

    auto configuration = [&] {
      QJsonObject res;
      for(auto kv : settingsJson(*fact, widget->getSettings())["Configuration"].toArray())
      {
        auto pair = kv.toArray();
        // ossia values are stored as {"Type": value}
        if(pair.size() == 2)
        {
          auto v = pair[1].toObject();
          res[pair[0].toString()] = v.isEmpty() ? QJsonValue{} : *v.begin();
        }
      }
      return res;
    };
    // "late" is saved by the module a while after init
    REQUIRE(waitFor([&] { return configuration().contains("late"); }));

    const auto conf = configuration();
    CHECK(conf["port"] == 1234);
    CHECK(!conf.contains("txPort"));
    CHECK(conf["iface"] == "Pick one");
    CHECK(conf["mode"] == 2);
    // Untouched values stay as the module wrote them, or undefined
    CHECK(conf["poll"] == 3);
    CHECK(conf["late"] == "learnt");
    CHECK(!conf.contains("debug"));
    CHECK(conf["weird"] == 2);
  });
}
