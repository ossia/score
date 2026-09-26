// Drives one companion module through score's Bitfocus host: the settings
// widget computes the configuration a user would save, then a device is built
// from it as BitfocusDevice::reconnect does, and every action of the tree is
// triggered. Used by tests/tools/bitfocus/sweep.py to compare score with a
// reference host reproducing companion.
#include <Protocols/Bitfocus/BitfocusContext.hpp>
#include <Protocols/Bitfocus/BitfocusProtocol.hpp>
#include <Protocols/Bitfocus/BitfocusProtocolFactory.hpp>
#include <Protocols/Bitfocus/BitfocusProtocolSettingsWidget.hpp>
#include <Protocols/Bitfocus/BitfocusSpecificSettings.hpp>

#include <ossia/detail/algorithms.hpp>
#include <ossia/network/base/node_attributes.hpp>
#include <ossia/network/common/complex_type.hpp>
#include <ossia/network/value/format_value.hpp>

#include <ossia-qt/js_utilities.hpp>

#include <score_test/App.hpp>

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QUdpSocket>

#include <chrono>
#include <cstdio>

namespace bitfocus
{
QString nodeExecutable(const QString& nodeVersion)
{
  const QString root = qEnvironmentVariable("BITFOCUS_NODE_RUNTIME");
  for(const auto& dir : QDir{root}.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    if("node" + dir.split('.').front() == nodeVersion)
      return root + "/" + dir + "/bin/node";
  return "node";
}
}

namespace
{
FILE* g_out{};

void emit(QJsonObject o)
{
  o["t"] = std::chrono::duration<double>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
  auto str = QJsonDocument{o}.toJson(QJsonDocument::Compact);
  std::fprintf(g_out, "%s\n", str.constData());
  std::fflush(g_out);
}

bool waitFor(const std::function<bool()>& pred, int ms)
{
  QElapsedTimer t;
  t.start();
  while(!pred() && t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
  return pred();
}

void sleepFor(int ms)
{
  waitFor([] { return false; }, ms);
}

QJsonValue toJson(const ossia::value& v)
{
  return QJsonValue::fromVariant(v.apply(ossia::qt::ossia_to_qvariant{}));
}

Protocols::BitfocusSpecificSettings settingsFromManifest(const QString& path)
{
  Protocols::BitfocusSpecificSettings set;
  QFile f{path + "/companion/manifest.json"};
  f.open(QIODevice::ReadOnly);
  auto doc = QJsonDocument::fromJson(f.readAll()).object();
  auto rt = doc["runtime"].toObject();
  set.path = path;
  set.id = doc["id"].toString();
  set.name = doc["shortname"].toString();
  set.brand = doc["manufacturer"].toString();
  set.nodeVersion = rt["type"].toString() == "node18" ? "node18" : "node22";
  set.apiVersion = rt["apiVersion"].toString();
  set.entrypoint = rt["entrypoint"].toString();
  set.entrypoint.remove("../");
  if(auto prods = doc["products"].toArray(); !prods.isEmpty())
    set.product = prods.first().toString();
  return set;
}

QJsonObject describeNode(ossia::net::node_base& n)
{
  QJsonObject o;
  o["name"] = QString::fromStdString(n.get_name());
  if(auto p = n.get_parameter())
  {
    o["type"] = int(p->get_value_type());
    o["value"] = toJson(p->value());
  }
  QJsonArray cld;
  for(auto& c : n.children())
    cld.append(describeNode(*c));
  if(!cld.isEmpty())
    o["children"] = cld;
  return o;
}
}

int run(int argc, char** argv)
{

  QString modulePath, outPath, configPath, scenarioPath;
  int settle = 2500, gap = 120;
  for(int i = 1; i + 1 < argc; i += 2)
  {
    QString k = argv[i], v = argv[i + 1];
    if(k == "--module")
      modulePath = v;
    else if(k == "--out")
      outPath = v;
    else if(k == "--config")
      configPath = v;
    else if(k == "--settle")
      settle = v.toInt();
    else if(k == "--gap")
      gap = v.toInt();
    else if(k == "--scenario")
      scenarioPath = v;
  }
  g_out = std::fopen(outPath.toUtf8().constData(), "w");
  if(!g_out)
    return 1;

  // Values the user types in the settings dialog: the host fields.
  QJsonObject overrides;
  {
    QFile f{configPath};
    if(f.open(QIODevice::ReadOnly))
      overrides = QJsonDocument::fromJson(f.readAll()).object()["overrides"].toObject();
  }

  // Explicit config values, actions with options, and packets from the device
  QJsonObject scenario;
  {
    QFile f{scenarioPath};
    if(f.open(QIODevice::ReadOnly))
      scenario = QJsonDocument::fromJson(f.readAll()).object();
    auto conf = scenario["config"].toObject();
    for(auto it = conf.begin(); it != conf.end(); ++it)
      overrides[it.key()] = it.value();
  }

  auto base = settingsFromManifest(modulePath);
  const QString deviceName = "dev";

  // 1. Settings dialog
  Protocols::BitfocusSpecificSettings saved;
  {
    auto stgs = base;

    Device::DeviceSettings ds;
    ds.name = deviceName;
    ds.protocol = Protocols::BitfocusProtocolFactory::static_concreteKey();
    ds.deviceSpecificSettings = QVariant::fromValue(stgs);

    Protocols::BitfocusProtocolSettingsWidget w;
    w.setSettings(ds);
    auto handler
        = w.getSettings().deviceSpecificSettings.value<Protocols::BitfocusSpecificSettings>().handler;
    // As a user would: the dialog is read once the module has initialised
    bool inited = false;
    if(handler)
      handler->afterRegistration([&inited] { inited = true; });
    bool ok = handler && waitFor([&] { return inited; }, 15000);
    QCoreApplication::processEvents();
    emit({{"ev", "widget-fields"}, {"ok", ok}});

    saved = w.getSettings().deviceSpecificSettings.value<Protocols::BitfocusSpecificSettings>();
    // What the user types in the dialog
    for(auto it = overrides.begin(); it != overrides.end(); ++it)
    {
      auto v = ossia::qt::qt_to_ossia{}(it.value().toVariant());
      auto cur = ossia::find_if(
          saved.configuration, [&](auto& kv) { return kv.first == it.key(); });
      if(cur != saved.configuration.end())
        cur->second = v;
      else
        saved.configuration.emplace_back(it.key(), v);
    }
    QJsonObject conf;
    for(auto& [k, v] : saved.configuration)
      conf[k] = toJson(v);
    emit(
        {{"ev", "widget-config"},
         {"config", conf},
         {"upgradeIndex", saved.upgradeIndex ? QJsonValue(*saved.upgradeIndex) : QJsonValue()}});
    saved.handler.reset();
  }
  sleepFor(300);

  // 2. The device, as BitfocusDevice::reconnect builds it from saved settings
  auto ctx = std::make_shared<ossia::net::network_context>();
  QTimer poll;
  QObject::connect(&poll, &QTimer::timeout, [&] { ctx->context.poll(); });
  poll.start(5);

  std::shared_ptr<bitfocus::module_handler> handler;
  std::shared_ptr<ossia::net::generic_device> dev;
  bool registered = false;
  {
    auto stgs = saved;
    stgs.deduplicateConfiguration();
    handler = stgs.makeHandler(deviceName);


    auto proto = std::make_unique<ossia::net::bitfocus_protocol>(handler, ctx);
    auto pproto = proto.get();
    dev = std::make_shared<ossia::net::generic_device>(
        std::move(proto), deviceName.toStdString());
    handler->afterRegistration([&registered, pproto] {
      pproto->init_device();
      registered = true;
    });
  }

  int varSignals = 0, fbSignals = 0;
  QObject::connect(
      handler.get(), &bitfocus::module_handler::variableChanged, [&] { varSignals++; });
  QObject::connect(
      handler.get(), &bitfocus::module_handler::feedbackValueChanged,
      [&] { fbSignals++; });

  waitFor([&] { return registered; }, 20000);
  emit({{"ev", "registered"}, {"ok", registered}});
  sleepFor(settle);

  auto& root = dev->get_root_node();
  emit({{"ev", "tree"}, {"root", describeNode(root)}});

  if(auto actions = root.find_child(std::string_view("action"));
     actions && scenario.contains("actions"))
  {
    emit({{"ev", "actions-begin"}});
    for(auto a : scenario["actions"].toArray())
    {
      const auto obj = a.toObject();
      const auto id = obj["id"].toString();
      auto node = actions->find_child(id.toStdString());
      if(!node || !node->get_parameter())
        continue;
      // A fresh companion action instance starts from the defaults
      if(auto def = handler->model().actions.find(id); def != handler->model().actions.end())
        for(auto& opt : def->second.options)
          if(auto cld = node->find_child(opt.id.toStdString()))
            if(auto p = cld->get_parameter())
              p->set_value(ossia::net::bitfocus_protocol::optionDefault(opt));
      const auto opts = obj["options"].toObject();
      for(auto it = opts.begin(); it != opts.end(); ++it)
        if(auto cld = node->find_child(it.key().toStdString()))
          if(auto p = cld->get_parameter())
            p->set_value(ossia::qt::qt_to_ossia{}(it.value().toVariant()));
      emit({{"ev", "action"}, {"id", id}});
      node->get_parameter()->push_value(ossia::impulse{});
      sleepFor(gap);
    }
    QUdpSocket inj;
    for(auto i : scenario["inject"].toArray())
    {
      const auto obj = i.toObject();
      inj.writeDatagram(
          QByteArray::fromHex(obj["hex"].toString().toLatin1()), QHostAddress::LocalHost,
          obj["port"].toInt());
      sleepFor(obj["wait"].toInt(300));
    }
    emit({{"ev", "actions-end"}});
    sleepFor(5000);
  }
  else if(actions)
  {
    emit({{"ev", "actions-begin"}});
    std::vector<ossia::net::node_base*> nodes;
    for(auto& c : actions->children())
      nodes.push_back(c.get());
    for(auto* node : nodes)
    {
      auto p = node->get_parameter();
      if(!p)
        continue;
      emit({{"ev", "action"}, {"id", QString::fromStdString(node->get_name())}});
      p->push_value(ossia::impulse{});
      sleepFor(gap);
    }
    emit({{"ev", "actions-end"}});
    // As long as the reference waits for the replies
    sleepFor(5000);
  }

  QJsonObject vars;
  if(auto variables = root.find_child(std::string_view("variable")))
    for(auto& c : variables->children())
      if(auto p = c->get_parameter())
        vars[QString::fromStdString(c->get_name())] = toJson(p->value());

  const auto& m = handler->model();
  emit(
      {{"ev", "summary"},
       {"actions", int(m.actions.size())},
       {"feedbacks", int(m.feedbacks.size())},
       {"variables", int(m.variables.size())},
       {"variableValues", vars},
       {"varSignals", varSignals},
       {"fbSignals", fbSignals},
       {"tree", describeNode(root)}});

  dev.reset();
  handler.reset();
  std::fclose(g_out);
  return 0;
}

int main(int argc, char** argv)
{
  // run_in_app changes the working directory: paths are made absolute first
  std::vector<QByteArray> storage;
  std::vector<char*> args{argv[0]};
  for(int i = 1; i < argc; i++)
  {
    const bool isPath = i > 1 && QByteArray(argv[i - 1]) != "--gap"
                        && QByteArray(argv[i - 1]) != "--settle"
                        && QByteArray(argv[i - 1]).startsWith("--");
    storage.push_back(
        isPath ? QFileInfo(QString::fromLocal8Bit(argv[i])).absoluteFilePath().toLocal8Bit()
               : QByteArray(argv[i]));
  }
  for(auto& a : storage)
    args.push_back(a.data());
  argc = int(args.size());
  argv = args.data();

  int ret = 1;
  score::test::run_in_app([&](const score::GUIApplicationContext&) { ret = run(argc, argv); });
  return ret;
}
