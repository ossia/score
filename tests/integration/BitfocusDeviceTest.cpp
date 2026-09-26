// A Bitfocus companion device in a document, as loading a saved file creates
// it: the settings come from JSON, the module (a mock, see
// src/plugins/score-plugin-protocols/Tests/data/bitfocus-mock) registers
// asynchronously, and the explorer must follow the tree the module defines,
// then and later.
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

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

#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QLineEdit>
#include <QStandardPaths>
#include <QTemporaryDir>

#include <catch2/catch_test_macros.hpp>

#include <functional>

namespace
{
constexpr auto bitfocusKey = "303993ed-b39a-4edb-90a6-2a3ae45043c4";

bool waitFor(const std::function<bool()>& pred, int ms = 15000)
{
  QElapsedTimer t;
  t.start();
  while(!pred() && t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
  return pred();
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
  if(QStandardPaths::findExecutable("node").isEmpty())
    SKIP("node is not installed");

  QTemporaryDir logDir;
  const QString logPath = logDir.path() + "/mock.jsonl";
  qputenv("MOCK_LOG", logPath.toUtf8());
  struct unset_log
  {
    ~unset_log() { qunsetenv("MOCK_LOG"); }
  } unset;
  auto mockEvents = [&](const QString& ev) {
    std::vector<QJsonObject> res;
    QFile f{logPath};
    if(f.open(QIODevice::ReadOnly))
      for(auto& line : f.readAll().split('\n'))
        if(auto o = QJsonDocument::fromJson(line).object(); o["ev"] == ev)
          res.push_back(o);
    return res;
  };

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& devplug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();

    auto* fact = ctx.interfaces<Device::ProtocolFactoryList>().get(
        UuidKey<Device::ProtocolFactory>::fromString(QString(bitfocusKey)));
    REQUIRE(fact);

    // As a document saved before the module had anything to save
    const auto json = QStringLiteral(R"({
      "Path": "%1", "Entrypoint": "main.js", "Identifier": "score-mock",
      "Name": "Mock", "Brand": "ossia", "Product": "", "NodeVersion": "node22",
      "APIVersion": "1.14.1", "Configuration": [], "Description": ""
    })").arg(SCORE_BITFOCUS_MOCK_DIR);
    auto json_doc = readJson(json.toUtf8());
    JSONWriter wrt{json_doc};

    Device::DeviceSettings set;
    set.name = "mock";
    set.protocol = fact->concreteKey();
    set.deviceSpecificSettings = fact->makeProtocolSpecificSettings(wrt.toVariant());

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
    REQUIRE(waitFor([&] {
      auto edits = widget->findChildren<QLineEdit*>();
      // The device name, then the fields: host is the first one
      if(edits.size() >= 2)
        hostEdit = edits[1];
      return hostEdit != nullptr;
    }));
    hostEdit->setText("10.1.2.3");
    device->updateSettings(widget->getSettings());

    CHECK(waitFor([&] { return mockEvents("updateConfigAndLabel").size() == 1; }));
    const auto upd = mockEvents("updateConfigAndLabel");
    if(!upd.empty())
      CHECK(upd[0]["msg"]["config"]["host"] == "10.1.2.3");
    CHECK(mockEvents("init").size() == 1);
    CHECK(waitFor([&] { return findPath(*devNode, {"action", "typed", "int"}) != nullptr; }));
    auto* typed = ossia::net::find_node(device->getDevice()->get_root_node(), "/action/typed");
    REQUIRE(typed);
    typed->get_parameter()->push_value(ossia::impulse{});
    CHECK(waitFor([&] { return mockEvents("executeAction").size() >= 2; }));
  });
}

TEST_CASE("a loaded Bitfocus device keeps what the module upgraded", "[integration][bitfocus]")
{
  if(QStandardPaths::findExecutable("node").isEmpty())
    SKIP("node is not installed");

  QTemporaryDir logDir;
  const QString logPath = logDir.path() + "/mock.jsonl";
  qputenv("MOCK_LOG", logPath.toUtf8());
  struct unset_log
  {
    ~unset_log() { qunsetenv("MOCK_LOG"); }
  } unset;
  auto mockEvents = [&](const QString& ev) {
    std::vector<QJsonObject> res;
    QFile f{logPath};
    if(f.open(QIODevice::ReadOnly))
      for(auto& line : f.readAll().split('\n'))
        if(auto o = QJsonDocument::fromJson(line).object(); o["ev"] == ev)
          res.push_back(o);
    return res;
  };

  score::test::run_in_app([&](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& devplug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* fact = ctx.interfaces<Device::ProtocolFactoryList>().get(
        UuidKey<Device::ProtocolFactory>::fromString(QString(bitfocusKey)));
    REQUIRE(fact);

    const auto json = QStringLiteral(R"({
      "Path": "%1", "Entrypoint": "main.js", "Identifier": "score-mock",
      "Name": "Mock", "Brand": "ossia", "Product": "", "NodeVersion": "node22",
      "APIVersion": "1.14.1", "Description": "", "UpgradeIndex": 2,
      "Configuration": [["host", {"String": "10.0.0.1"}], ["version", {"String": "old"}]]
    })").arg(SCORE_BITFOCUS_MOCK_DIR);
    auto json_doc = readJson(json.toUtf8());
    JSONWriter wrt{json_doc};
    Device::DeviceSettings set;
    set.name = "mock";
    set.protocol = fact->concreteKey();
    set.deviceSpecificSettings = fact->makeProtocolSpecificSettings(wrt.toVariant());
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
    QElapsedTimer t;
    t.start();
    while(t.elapsed() < 500)
      QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    CHECK(mockEvents("updateConfigAndLabel").empty());
    CHECK(mockEvents("init").size() == 1);
    CHECK(std::as_const(mockEvents("init")[0])["msg"]["isFirstInit"] == false);
    CHECK(versionOf() == R"({"String":"new"})");

    // A module without configuration was initialised before: not a new connection
    const auto json2 = QStringLiteral(R"({
      "Path": "%1", "Entrypoint": "main.js", "Identifier": "score-mock",
      "Name": "Mock", "Brand": "ossia", "Product": "", "NodeVersion": "node22",
      "APIVersion": "1.14.1", "Description": "", "UpgradeIndex": 3, "Configuration": []
    })").arg(SCORE_BITFOCUS_MOCK_DIR);
    auto json_doc2 = readJson(json2.toUtf8());
    JSONWriter wrt2{json_doc2};
    Device::DeviceSettings set2;
    set2.name = "mock2";
    set2.protocol = fact->concreteKey();
    set2.deviceSpecificSettings = fact->makeProtocolSpecificSettings(wrt2.toVariant());
    disp.submit(new Explorer::Command::LoadDevice{devplug, std::move(set2)});
    REQUIRE(waitFor([&] { return mockEvents("init").size() == 2; }));
    CHECK(std::as_const(mockEvents("init")[1])["msg"]["isFirstInit"] == false);
  });
}

TEST_CASE("the Bitfocus settings dialog saves what companion would", "[integration][bitfocus]")
{
  if(QStandardPaths::findExecutable("node").isEmpty())
    SKIP("node is not installed");

  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* fact = ctx.interfaces<Device::ProtocolFactoryList>().get(
        UuidKey<Device::ProtocolFactory>::fromString(QString(bitfocusKey)));
    REQUIRE(fact);

    // A device picked in the list, before anything is configured
    const auto json = QStringLiteral(R"({
      "Path": "%1", "Entrypoint": "main.js", "Identifier": "score-mock",
      "Name": "Mock", "Brand": "ossia", "Product": "", "NodeVersion": "node22",
      "APIVersion": "1.14.1", "Configuration": [], "Description": ""
    })").arg(SCORE_BITFOCUS_MOCK_DIR);
    auto json_doc = readJson(json.toUtf8());
    JSONWriter wrt{json_doc};

    Device::DeviceSettings set;
    set.name = "mock";
    set.protocol = fact->concreteKey();
    set.deviceSpecificSettings = fact->makeProtocolSpecificSettings(wrt.toVariant());

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
