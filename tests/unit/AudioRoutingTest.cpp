#include <Device/Address/AddressSettings.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Explorer/Commands/Add/AddAddress.hpp>
#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioDevice.hpp>
#include <Audio/AudioRouting.hpp>
#include <Audio/RoutingMatrix.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <ossia/audio/audio_parameter.hpp>
#include <ossia/audio/audio_protocol.hpp>
#include <ossia/network/base/node_functions.hpp>

#include <QCoreApplication>
#include <QMouseEvent>
#include <QElapsedTimer>

#include <catch2/catch_test_macros.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <limits>

namespace
{
using Audio::RoutingMatrix;
using Audio::unrouted;

std::vector<Audio::RoutedPort> twoPorts()
{
  return {{"a", {0, 1}}, {"b", {unrouted, unrouted, unrouted}}};
}

ossia::mapped_audio_parameter*
findMapped(Dataflow::AudioDevice& dev, const std::string& path)
{
  auto node = ossia::net::find_node(dev.getDevice()->get_root_node(), path);
  return node ? dynamic_cast<ossia::mapped_audio_parameter*>(node->get_parameter())
              : nullptr;
}

Device::AddressSettings
portSettings(const QString& name, const std::string& kind, ossia::audio_mapping m)
{
  Device::AddressSettings s;
  s.name = name;
  s.extendedAttributes["audio-kind"] = kind;
  s.extendedAttributes["audio-channels"] = int(m.size());
  s.extendedAttributes["audio-mapping"] = std::move(m);
  return s;
}

void addPorts(
    const score::DocumentContext& ctx, std::vector<Device::AddressSettings> ports)
{
  auto& plug = ctx.plugin<Explorer::DeviceDocumentPlugin>();
  auto* dev = plug.list().audioDevice();
  RedoMacroCommandDispatcher<Explorer::Command::AddAddresses> disp{ctx.commandStack};
  disp.submit(new Explorer::Command::LoadDevice{plug, dev->settings()});
  Device::Node* node{};
  for(auto& n : plug.rootNode())
    if(n.is<Device::DeviceSettings>()
       && n.get<Device::DeviceSettings>().name == dev->settings().name)
      node = &n;
  REQUIRE(node);
  for(auto& p : ports)
    disp.submit(new Explorer::Command::AddAddress{
        plug, Device::NodePath{*node}, InsertMode::AsChild, p});
  disp.commit();
}

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}
}

TEST_CASE("The routing matrix edits the mappings of its ports", "[audio][routing]")
{
  score::test::run_in_app([](const score::GUIApplicationContext&) {
    RoutingMatrix m{RoutingMatrix::Direction::ToCard};
    m.setCardChannels(4);
    m.setPorts(twoPorts());
    REQUIRE(m.model()->rowCount() == 5);
    REQUIRE(m.model()->columnCount() == 4);

    SECTION("a click connects a channel, a second one disconnects it")
    {
      m.toggle(2, 3);
      CHECK(m.ports()[1].mapping == ossia::int_vector{3, unrouted, unrouted});
      m.toggle(2, 3);
      CHECK(m.ports()[1].mapping == ossia::int_vector{unrouted, unrouted, unrouted});
      m.toggle(0, 2);
      CHECK(m.ports()[0].mapping == ossia::int_vector{2, 1});
    }

    SECTION("the diagonal stops at the end of the port")
    {
      m.connectDiagonally(2, 0, false);
      CHECK(m.ports()[0].mapping == ossia::int_vector{0, 1});
      CHECK(m.ports()[1].mapping == ossia::int_vector{0, 1, 2});
    }

    SECTION("across ports, the diagonal stops at the last channel of the card")
    {
      m.connectDiagonally(1, 1, true);
      CHECK(m.ports()[0].mapping == ossia::int_vector{0, 1});
      CHECK(m.ports()[1].mapping == ossia::int_vector{2, 3, unrouted});
    }

    SECTION("a channel, a port, or everything is disconnected")
    {
      m.disconnectRow(1);
      CHECK(m.ports()[0].mapping == ossia::int_vector{0, unrouted});
      m.connectDiagonally(2, 0, false);
      m.disconnectPort(1);
      CHECK(m.ports()[1].mapping == ossia::int_vector{unrouted, unrouted, unrouted});
      m.disconnectAll();
      CHECK(m.ports()[0].mapping == ossia::int_vector{unrouted, unrouted});
    }

    SECTION("with a handler, a gesture is asked for, not applied")
    {
      std::vector<RoutingMatrix::Changes> asked;
      m.onRoute = [&](RoutingMatrix::Changes c) { asked.push_back(std::move(c)); };
      m.connectDiagonally(0, 1, true);
      REQUIRE(asked.size() == 1);
      REQUIRE(asked[0].size() == 2);
      CHECK(asked[0][0] == std::pair{0, ossia::int_vector{1, 2}});
      CHECK(asked[0][1] == std::pair{1, ossia::int_vector{3, unrouted, unrouted}});
      CHECK(m.ports()[0].mapping == ossia::int_vector{0, 1});

      m.disconnectAll();
      REQUIRE(asked.size() == 2);
      REQUIRE(asked[1].size() == 1);
      CHECK(asked[1][0] == std::pair{0, ossia::int_vector{unrouted, unrouted}});
    }

    SECTION("a channel sent beyond the card shows as going nowhere")
    {
      m.setCardChannels(2);
      auto fg = [&](int row) {
        return m.model()->headerData(row, Qt::Vertical, Qt::ForegroundRole).isValid();
      };
      CHECK(!fg(0));
      CHECK(!fg(1));
      m.setPorts({{"a", {0, 3}}});
      CHECK(!fg(0));
      CHECK(fg(1));
    }

    SECTION("the modifiers of a click pick the gesture")
    {
      m.resize(600, 400);
      m.show();
      auto click = [&](int row, int column, Qt::KeyboardModifiers mods) {
        const auto pos = QPointF(m.visualRect(m.model()->index(row, column)).center());
        QMouseEvent e{
            QEvent::MouseButtonPress, pos, m.viewport()->mapToGlobal(pos),
            Qt::LeftButton, Qt::LeftButton, mods};
        QCoreApplication::sendEvent(m.viewport(), &e);
      };
      click(2, 0, Qt::NoModifier);
      CHECK(m.ports()[1].mapping == ossia::int_vector{0, unrouted, unrouted});
      click(2, 0, Qt::ControlModifier);
      CHECK(m.ports()[1].mapping == ossia::int_vector{0, 1, 2});
      m.disconnectAll();
      click(1, 1, Qt::ControlModifier | Qt::ShiftModifier);
      CHECK(m.ports()[0].mapping == ossia::int_vector{unrouted, 1});
      CHECK(m.ports()[1].mapping == ossia::int_vector{2, 3, unrouted});
    }

    SECTION("new ports with the same shape keep the rows")
    {
      auto ports = twoPorts();
      ports[1].mapping = {1, 2, 3};
      m.setPorts(ports);
      CHECK(m.model()->rowCount() == 5);
      CHECK(m.model()->index(3, 2).data(Qt::UserRole + 1).toBool());
    }
  });
}

TEST_CASE("A routing gesture is one undo step", "[audio][routing]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto* dev = static_cast<Dataflow::AudioDevice*>(
        dctx.plugin<Explorer::DeviceDocumentPlugin>().list().audioDevice());
    REQUIRE(dev);
    REQUIRE(dev->getProtocol());

    addPorts(dctx, {portSettings("mic", "in", {0}), portSettings("pa", "out", {0, 1})});
    auto* mic = findMapped(*dev, "/mic");
    auto* pa = findMapped(*dev, "/pa");
    REQUIRE(mic);
    REQUIRE(pa);

    Audio::applyRouting(
        dctx, {{State::Address{"audio", {"mic"}}, {1}},
               {State::Address{"audio", {"pa"}}, {unrouted, 0}}});
    CHECK(mic->mapping == ossia::audio_mapping{1});
    CHECK(pa->mapping[0] == std::numeric_limits<int>::max());
    CHECK(pa->mapping[1] == 0);
    CHECK(findMapped(*dev, "/pa") == pa);

    auto node = Device::try_getNodeFromAddress(
        dctx.plugin<Explorer::DeviceDocumentPlugin>().rootNode(),
        State::Address{"audio", {"pa"}});
    REQUIRE(node);
    auto saved = ossia::any_cast<ossia::audio_mapping>(
        node->get<Device::AddressSettings>().extendedAttributes.at("audio-mapping"));
    CHECK(saved == ossia::audio_mapping{unrouted, 0});

    spin(100);

    doc->commandStack().undo();
    CHECK(mic->mapping == ossia::audio_mapping{0});
    CHECK(pa->mapping == ossia::audio_mapping{0, 1});
    doc->commandStack().redo();
    CHECK(mic->mapping == ossia::audio_mapping{1});
    CHECK(pa->mapping[1] == 0);
  });
}

TEST_CASE("The port dialog keeps what it does not edit", "[audio][routing]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);
    auto& dctx = doc->context();
    auto* dev = static_cast<Dataflow::AudioDevice*>(
        dctx.plugin<Explorer::DeviceDocumentPlugin>().list().audioDevice());
    REQUIRE(dev);
    auto proto
        = ctx.interfaces<Device::ProtocolFactoryList>().get(dev->settings().protocol);
    REQUIRE(proto);

    auto edit = [&](const Device::AddressSettings& s) {
      std::unique_ptr<Device::AddressDialog> dial{
          proto->makeEditAddressDialog(s, *dev, dctx, nullptr)};
      REQUIRE(dial);
      return dial->getSettings();
    };

    auto out = portSettings("pa", "out", {unrouted, 1});
    out.extendedAttributes["audio-gain"] = 0.5;
    auto res = edit(out);
    CHECK(
        ossia::any_cast<ossia::audio_mapping>(res.extendedAttributes.at("audio-mapping"))
        == ossia::audio_mapping{unrouted, 1});
    CHECK(ossia::any_cast<double>(res.extendedAttributes.at("audio-gain")) == 0.5);

    Device::AddressSettings virt;
    virt.name = "bus";
    virt.extendedAttributes["audio-kind"] = std::string{"virtual"};
    virt.extendedAttributes["audio-channels"] = 4;
    res = edit(virt);
    CHECK(
        ossia::any_cast<std::string>(res.extendedAttributes.at("audio-kind"))
        == "virtual");
    CHECK(ossia::any_cast<int>(res.extendedAttributes.at("audio-channels")) == 4);
    CHECK(!res.extendedAttributes.contains("audio-mapping"));
  });
}
