// The tracking protocols as devices in score: each one can be added to a
// document with its default settings, coexists with the others, and builds its
// tree when added.
//
// This is the check the "Add device" dialog runs before enabling its button,
// DeviceExplorerModel::checkDeviceInstantiatable: it compares the new settings
// with every device already present, and with *empty* settings. A factory
// whose checkCompatibility() compares ports without looking at the protocol
// first answers "conflict" to that empty-settings probe whenever the user kept
// the default port - which is how none of these devices could be added.

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <vector>

namespace
{
std::vector<Device::ProtocolFactory*> trackingFactories(const score::GUIApplicationContext& ctx)
{
  std::vector<Device::ProtocolFactory*> out;
  for(auto& f : ctx.interfaces<Device::ProtocolFactoryList>())
    if(f.category() == Device::ProtocolFactory::StandardCategories::tracking)
      out.push_back(&f);
  return out;
}
}

TEST_CASE("Every tracking protocol is compatible with empty settings and with the other protocols", "[tracking][devices]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto factories = trackingFactories(ctx);
    REQUIRE(factories.size() >= 4); // PSN, RTTrP, TUIO, OpenTrackIO (+ OpenXR when built)

    for(auto f : factories)
    {
      INFO(f->prettyName().toStdString());
      const auto& def = f->defaultSettings();
      CHECK(def.protocol == f->concreteKey());
      CHECK(!def.name.isEmpty());

      // The probe the explorer makes with empty settings
      CHECK(f->checkCompatibility(def, Device::DeviceSettings{}));

      // A second device of the same protocol with the same port: a conflict;
      // the same settings under another name still conflict on the port...
      auto twin = def;
      twin.name = def.name + "_2";
      CHECK(!f->checkCompatibility(def, twin));
      // ... and the very same device (same name) is never compatible with itself
      CHECK(!f->checkCompatibility(def, def));

      // Other protocols never conflict
      for(auto g : factories)
        if(g != f)
          CHECK(f->checkCompatibility(def, g->defaultSettings()));
    }
  });
}

TEST_CASE("Every tracking protocol can be added to an empty document", "[tracking][devices]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto& model = plug.explorer();

    for(auto f : trackingFactories(ctx))
    {
      INFO(f->prettyName().toStdString());
      CHECK(model.checkDeviceInstantiatable(f->defaultSettings()));
    }
  });
}

TEST_CASE("Adding the tracking devices builds their trees", "[tracking][devices]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto& model = plug.explorer();

    for(auto f : trackingFactories(ctx))
    {
      const auto name = f->prettyName().toStdString();
      INFO(name);
      // OpenXR needs a runtime: it is enough that it can be attempted
      const bool needs_hardware = name.find("OpenXR") != std::string::npos;

      auto settings = f->defaultSettings();
      REQUIRE(model.checkDeviceInstantiatable(settings));

      CommandDispatcher<>{doc->context().commandStack}.submit(
          new Explorer::Command::LoadDevice{plug, settings});

      auto dev = plug.list().findDevice(settings.name);
      REQUIRE(dev != nullptr);

      const Device::Node* node = nullptr;
      for(auto& n : plug.rootNode())
        if(n.get<Device::DeviceSettings>().name == settings.name)
          node = &n;
      REQUIRE(node != nullptr);

      if(!needs_hardware)
      {
        CHECK(dev->connected());
        CHECK(node->hasChildren());
      }

      // Once present, the very same settings are not instantiatable twice
      CHECK(!model.checkDeviceInstantiatable(settings));
    }
  });
}
