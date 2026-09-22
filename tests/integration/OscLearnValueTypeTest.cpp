// Integration test: what the device explorer ends up with when an OSC device
// learns its namespace from incoming traffic. A message with no argument is an
// impulse, one argument is its scalar type, two to four floats are a vector,
// and anything above that is a list -- at every size, with no ceiling.
//
// An address whose first message carried no argument keeps no type: the next
// message that does carry one types it. A sender that publishes an empty list
// until it has something to say must not pin its address to an impulse.

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Protocols/OSC/OSCSpecificSettings.hpp>

#include <Explorer/Commands/Add/LoadDevice.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <score/command/Dispatchers/CommandDispatcher.hpp>

#include <core/document/Document.hpp>

#include <catch2/catch_test_macros.hpp>

#include <QElapsedTimer>
#include <QHostAddress>
#include <QUdpSocket>
#include <QCoreApplication>

#include <cstring>

namespace
{
constexpr int osc_port = 42197;

void pad4(QByteArray& b)
{
  while(b.size() % 4 != 0)
    b.append('\0');
}

//! An OSC message with n float arguments.
QByteArray oscMessage(const QString& address, int n)
{
  QByteArray out;
  out.append(address.toUtf8());
  out.append('\0');
  pad4(out);

  QByteArray tags = ",";
  tags.append(QByteArray(n, 'f'));
  tags.append('\0');
  pad4(tags);
  out.append(tags);

  for(int i = 0; i < n; i++)
  {
    const float f = i + 1.f;
    uint32_t bits{};
    std::memcpy(&bits, &f, 4);
    for(int b = 3; b >= 0; b--)
      out.append(char((bits >> (8 * b)) & 0xFF));
  }
  return out;
}

Device::ProtocolFactory* oscFactory(const score::GUIApplicationContext& ctx)
{
  for(auto& f : ctx.interfaces<Device::ProtocolFactoryList>())
    if(f.prettyName() == "OSC")
      return &f;
  return nullptr;
}

Device::DeviceSettings learningDevice(Device::ProtocolFactory& fact)
{
  auto settings = fact.defaultSettings();
  settings.name = "learn";

  Protocols::OSCSpecificSettings specif;
  ossia::net::udp_configuration udp;
  udp.local = ossia::net::inbound_socket_configuration{"0.0.0.0", osc_port};
  udp.remote = ossia::net::outbound_socket_configuration{"127.0.0.1", osc_port + 1};
  specif.configuration.transport = udp;
  settings.deviceSpecificSettings = QVariant::fromValue(specif);
  return settings;
}

void spin(int ms)
{
  QElapsedTimer t;
  t.start();
  while(t.elapsed() < ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
}

const Device::Node* findChild(const Device::Node& dev, const QString& name)
{
  for(auto& c : dev)
    if(c.displayName() == name)
      return &c;
  return nullptr;
}
}

TEST_CASE("learning an OSC namespace keeps the type of each message", "[integration][osc][learn]")
{
  score::test::run_in_gui_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc != nullptr);

    auto* fact = oscFactory(ctx);
    REQUIRE(fact != nullptr);

    auto& devplug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();

    CommandDispatcher<> disp{doc->context().commandStack};
    disp.submit(new Explorer::Command::LoadDevice{
        devplug, Device::Node{learningDevice(*fact), nullptr}});

    auto* dev = devplug.list().findDevice("learn");
    REQUIRE(dev != nullptr);
    REQUIRE(dev->connected());

    dev->setLearning(true);

    QUdpSocket sock;
    const int counts[] = {0, 1, 2, 3, 4, 5, 8, 16, 64};
    for(int n : counts)
    {
      const auto msg = oscMessage(QString{"/n%1"}.arg(n), n);
      sock.writeDatagram(msg, QHostAddress{"127.0.0.1"}, osc_port);
      spin(30);
    }
    // An empty list travels as a message with no argument, which is exactly
    // what an impulse is: a sender with nothing to publish yet must not decide
    // the address's type for the rest of the session.
    sock.writeDatagram(oscMessage("/latched", 0), QHostAddress{"127.0.0.1"}, osc_port);
    spin(30);
    sock.writeDatagram(oscMessage("/latched", 42), QHostAddress{"127.0.0.1"}, osc_port);
    spin(30);

    spin(300);
    dev->setLearning(false);
    spin(100);

    const Device::Node* devNode{};
    for(auto& n : devplug.rootNode())
      if(n.get<Device::DeviceSettings>().name == "learn")
        devNode = &n;
    REQUIRE(devNode != nullptr);

    auto learned = [&](int n) {
      auto* c = findChild(*devNode, QString{"n%1"}.arg(n));
      REQUIRE(c != nullptr);
      return c->get<Device::AddressSettings>().value.get_type();
    };

    CHECK(learned(0) == ossia::val_type::IMPULSE);
    CHECK(learned(1) == ossia::val_type::FLOAT);
    CHECK(learned(2) == ossia::val_type::VEC2F);
    CHECK(learned(3) == ossia::val_type::VEC3F);
    CHECK(learned(4) == ossia::val_type::VEC4F);
    CHECK(learned(5) == ossia::val_type::LIST);
    CHECK(learned(8) == ossia::val_type::LIST);
    CHECK(learned(16) == ossia::val_type::LIST);
    CHECK(learned(64) == ossia::val_type::LIST);

    auto* latched = findChild(*devNode, "latched");
    REQUIRE(latched != nullptr);
    CHECK(latched->get<Device::AddressSettings>().value.get_type()
          == ossia::val_type::LIST);
  });
}
