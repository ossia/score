// Settings a machine carried without understanding them, read by one that does.
//
// A terminal has no factory for most protocols, so it never decodes their
// settings: it keeps the bytes and hands them back when adding the device. The
// host then reconstructs them with the real protocol. That is the only path by
// which a device gets added from a terminal, and it aborted.
//
// The reason is a name collision. The protocol writes its settings into the
// *same* JSON object as the device's own "Name" and "Protocol", and evdev calls
// one of its own settings "Name". Building the opaque payload strips the
// members score owns -- so that writing it back cannot duplicate them -- and
// that took the protocol's "Name" with it. rapidjson's operator[] on a missing
// member asserts, so the host died reading a device the terminal sent.

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/OpaquePayload.hpp>

#include <core/document/Document.hpp>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
//! One enumerated device per protocol that has any. Every protocol, not the
//! first with a "Name" in its JSON -- score writes a "Name" for the device
//! itself, so that test matches everything and proves nothing. Only some
//! protocols name a setting of their own the same way, and those are the ones
//! that broke.
std::vector<std::pair<QString, Device::DeviceSettings>>
enumeratedDevices(const score::GUIApplicationContext& ctx, score::Document& doc)
{
  std::vector<std::pair<QString, Device::DeviceSettings>> out;
  for(auto& factory : ctx.interfaces<Device::ProtocolFactoryList>())
  {
    std::optional<Device::DeviceSettings> found;
    for(auto [category, enumerator] : factory.getEnumerators(doc.context()))
    {
      std::unique_ptr<Device::DeviceEnumerator> owned{enumerator};
      if(owned && !found)
        owned->enumerate([&](const QString&, const Device::DeviceSettings& s) {
          if(!found)
            found = s;
        });
    }
    if(found)
      out.emplace_back(factory.prettyName(), *found);
  }
  return out;
}

//! What a machine without the factory ends up holding: the object minus the
//! members score owns.
QByteArray strippedPayload(const Device::DeviceSettings& s)
{
  JSONReader r;
  r.readFrom(s);
  const auto bytes = r.toByteArray();

  rapidjson::Document doc;
  doc.Parse(bytes.constData(), bytes.size());
  REQUIRE(!doc.HasParseError());

  // The real stripping, not a hand-rolled RemoveMember: that drops only the
  // first member of a name, and the protocol writes into the same object, so
  // its own "Name" is a second one that survives. Skipping every member of the
  // name -- what score actually does -- is what loses it.
  const QStringList owned{
      QString::fromStdString(score::StringConstant().Name),
      QString::fromStdString(score::StringConstant().Protocol)};
  return score::OpaquePayload::fromJson(doc, owned).toBlob();
}
}

TEST_CASE("Settings carried without being understood survive the trip", "[devices]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* doc = score::test::new_document(ctx);
    REQUIRE(doc);

    const auto devices = enumeratedDevices(ctx, *doc);
    if(devices.empty())
    {
      WARN("nothing is plugged into this machine; nothing to carry");
      return;
    }

    for(const auto& [protocolName, device] : devices)
    {
      INFO("protocol: " << protocolName.toStdString());
      const auto* original = &device;

      // As a terminal holds it: the name and the protocol, and the rest as
      // bytes it never looked inside.
      Device::DeviceSettings carried;
      carried.name = original->name;
      carried.protocol = original->protocol;
      carried.opaqueSettings = strippedPayload(*original);
      REQUIRE(!carried.opaqueSettings.isEmpty());

    // Sent to the machine that will make the device, which does have the
    // protocol. This aborted: the protocol asked for a "Name" that stripping
    // had removed.
      // Written by hand, because this process *has* the protocol: readFrom
      // would re-encode from deviceSpecificSettings and the carried bytes would
      // never travel, which is exactly the case that cannot fail. This is the
      // wire a machine without the factory puts out -- name, protocol, and the
      // blob it never opened.
      QByteArray wire;
      {
        DataStream::Serializer s{&wire};
        s.stream() << carried.name << carried.protocol << carried.opaqueSettings;
        s.insertDelimiter();
      }

      Device::DeviceSettings received;
      {
        DataStream::Deserializer d{wire};
        d.writeTo(received);
      }

      CHECK(received.name == original->name);
      CHECK(received.protocol == original->protocol);
      REQUIRE(received.deviceSpecificSettings.isValid());

      // The content, not merely that something came back. A missing member
      // does not always abort -- with assertions compiled out, rapidjson hands
      // back a garbage value and the device is made from nonsense instead.
      // Comparing what the protocol writes for each is the only check that
      // sees the difference.
      JSONReader before, after;
      before.readFrom(*original);
      after.readFrom(received);
      CHECK(before.toByteArray() == after.toByteArray());
    }
  });
}
