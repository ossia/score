// App test: a mapped or virtual port of the audio device is edited in place,
// while the engine runs. Running processes may hold a pointer to the port, so
// it has to stay the same object.

#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>

#include <Explorer/Commands/Add/AddAddress.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioDevice.hpp>
#include <Audio/PortGain.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <ossia/audio/audio_engine.hpp>
#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/detail/algorithms.hpp>
#include <ossia/network/base/node_functions.hpp>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

namespace
{
Device::FullAddressSettings
port(const QString& name, const std::string& kind, ossia::audio_mapping mapping)
{
  Device::FullAddressSettings s;
  s.address = State::Address{"audio", {name}};
  s.extendedAttributes["audio-kind"] = kind;
  s.extendedAttributes["audio-mapping"] = std::move(mapping);
  return s;
}

Device::FullAddressSettings virtual_port(const QString& name, int channels)
{
  Device::FullAddressSettings s;
  s.address = State::Address{"audio", {name}};
  s.extendedAttributes["audio-kind"] = std::string{"virtual"};
  s.extendedAttributes["audio-channels"] = channels;
  return s;
}

ossia::audio_parameter* find(Dataflow::AudioDevice& dev, const std::string& path)
{
  auto node = ossia::net::find_node(dev.getDevice()->get_root_node(), path);
  return node ? static_cast<ossia::audio_parameter*>(node->get_parameter()) : nullptr;
}
}

TEST_CASE("A port of the audio device is edited in place", "[audio][ports]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    REQUIRE(ctx.guiApplicationPlugin<Audio::ApplicationPlugin>().audio);
    auto* dev = static_cast<Dataflow::AudioDevice*>(
        doc->context().plugin<Explorer::DeviceDocumentPlugin>().list().audioDevice());
    REQUIRE(dev);
    REQUIRE(dev->getProtocol());
    auto& proto = *dev->getProtocol();

    int ports_changed = 0;
    QObject::connect(
        dev, &Dataflow::AudioDevice::portsChanged, [&] { ports_changed++; });

    SECTION("a mapped output gets a new mapping, then becomes an input")
    {
      dev->addAddress(port("fx", "out", {0, 1}));
      auto* p = dynamic_cast<ossia::mapped_audio_parameter*>(find(*dev, "/fx"));
      REQUIRE(p);
      CHECK(ossia::contains(proto.out_mappings, p));

      dev->updateAddress(State::Address{"audio", {"fx"}}, port("fx", "out", {1}));
      CHECK(find(*dev, "/fx") == p);
      CHECK(p->mapping == ossia::audio_mapping{1});

      dev->updateAddress(State::Address{"audio", {"fx"}}, port("fx", "in", {0}));
      CHECK(find(*dev, "/fx") == p);
      CHECK(!p->is_output);
      CHECK(!ossia::contains(proto.out_mappings, p));
      CHECK(ossia::contains(proto.in_mappings, p));
      CHECK(p->upstream == proto.main_audio_in);

      dev->removeNode(State::Address{"audio", {"fx"}});
      CHECK(!find(*dev, "/fx"));
      CHECK(!ossia::contains(proto.in_mappings, p));
      CHECK(ports_changed == 4);
    }

    SECTION("a virtual port gets a new channel count")
    {
      dev->addAddress(virtual_port("bus", 2));
      auto* p = dynamic_cast<ossia::virtual_audio_parameter*>(find(*dev, "/bus"));
      REQUIRE(p);

      dev->updateAddress(State::Address{"audio", {"bus"}}, virtual_port("bus", 6));
      CHECK(find(*dev, "/bus") == p);
      CHECK(p->audio.size() == 6);
    }
  });
}

TEST_CASE("A port is added the way the mixer adds it", "[audio][ports]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* dev = static_cast<Dataflow::AudioDevice*>(plug.list().audioDevice());
    REQUIRE(dev);

    auto explorer_node = [&]() -> Device::Node* {
      for(auto& n : plug.rootNode())
        if(n.is<Device::DeviceSettings>()
           && n.get<Device::DeviceSettings>().name == dev->settings().name)
          return &n;
      return nullptr;
    };
    CHECK(!explorer_node());

    auto stgs = Device::AddressSettings{};
    stgs.name = "rev";
    stgs.extendedAttributes["audio-kind"] = std::string{"out"};
    stgs.extendedAttributes["audio-mapping"] = ossia::audio_mapping{1, 0};

    RedoMacroCommandDispatcher<Explorer::Command::AddAddresses> disp{
        doc->context().commandStack};
    disp.submit(new Explorer::Command::LoadDevice{plug, dev->settings()});
    auto node = explorer_node();
    REQUIRE(node);
    disp.submit(new Explorer::Command::AddAddress{
        plug, Device::NodePath{*node}, InsertMode::AsChild, stgs});
    disp.commit();

    CHECK(find(*dev, "/rev"));
  });
}

TEST_CASE("A port's gain is undoable and saved with the document", "[audio][ports][gain]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* dev = static_cast<Dataflow::AudioDevice*>(plug.list().audioDevice());
    REQUIRE(dev);
    auto* master = find(*dev, "/out/main");
    REQUIRE(master);

    // What a fader does: the gain changes live, then the release records it.
    master->push_value(0.5f);
    Audio::commitPortGain(
        doc->context(), State::Address{"audio", {"out", "main"}}, 1., 0.5);
    // Showing the device in the explorer does not rebuild it.
    CHECK(find(*dev, "/out/main") == master);
    auto gain = [&] { return master->gain(); };
    CHECK(gain() == Catch::Approx(0.5));
    auto node = Device::try_getNodeFromAddress(
        plug.rootNode(), State::Address{"audio", {"out", "main"}});
    REQUIRE(node);
    REQUIRE(node->is<Device::AddressSettings>());
    auto& attrs = node->get<Device::AddressSettings>().extendedAttributes;
    REQUIRE(attrs.contains("audio-gain"));

    doc->commandStack().undo();
    CHECK(gain() == Catch::Approx(1.));
    doc->commandStack().redo();
    CHECK(gain() == Catch::Approx(0.5));

    auto* reloaded = score::test::reload_via_json(ctx, *doc);
    REQUIRE(reloaded);
    auto& rplug = reloaded->context().plugin<Explorer::DeviceDocumentPlugin>();
    auto* rdev = static_cast<Dataflow::AudioDevice*>(rplug.list().audioDevice());
    REQUIRE(rdev);
    auto* rmaster = find(*rdev, "/out/main");
    REQUIRE(rmaster);
    CHECK(rmaster->gain() == Catch::Approx(0.5));
  });
}
