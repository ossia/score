// A terminal has no device objects, so the port inspector cannot find out what
// a device can be plugged into by casting it. It uses the kinds the machine
// running the score reported instead.

#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Process/Dataflow/PortAddressComboBox.hpp>
#include <Process/Dataflow/Port.hpp>

#include <State/Address.hpp>

#include <score/document/DocumentContext.hpp>
#include <score/document/DocumentRole.hpp>

#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <QComboBox>

#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <catch2/catch_test_macros.hpp>

namespace
{
QByteArray emptyDocument(const score::GUIApplicationContext& ctx)
{
  auto* doc = score::test::new_document(ctx);
  SCORE_ASSERT(doc);
  JSONObject::Serializer wr{};
  doc->saveAsJson(wr);
  return wr.toByteArray();
}

score::Document* reload(
    const score::GUIApplicationContext& ctx, const QByteArray& bytes,
    score::DocumentRole role)
{
  auto& delegates = ctx.interfaces<score::DocumentDelegateList>();
  SCORE_ASSERT(!delegates.empty());
  auto* doc = ctx.docManager.loadDocument(
      ctx, QStringLiteral("terminal"), bytes, JSONObject::type(), *delegates.begin(),
      role);
  QApplication::processEvents();
  return doc;
}

std::vector<QString> entries(const QComboBox& box)
{
  std::vector<QString> res;
  for(int i = 0; i < box.count(); i++)
    res.push_back(box.itemText(i));
  return res;
}

// The combo lists addresses. Only the device is known here -- its tree lives
// on the machine that holds it -- so a reported device shows up as "name:/".
bool has(const QComboBox& box, const QString& device)
{
  const auto want = State::Address{device, {}}.toString();
  const auto e = entries(box);
  return std::find(e.begin(), e.end(), want) != e.end();
}
}

TEST_CASE("A terminal's port combo offers the devices the score's machine has", "[terminal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    const auto bytes = emptyDocument(ctx);

    auto* doc = reload(ctx, bytes, score::DocumentRole::Terminal);
    REQUIRE(doc);
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();

    // Nothing was instantiated here, which is the whole difficulty: the combo
    // has no object to ask.
    REQUIRE(plug.list().devices().empty());

    plug.setRemoteKinds(QStringLiteral("stagewindow"), Device::NodeKind::TextureOut);
    plug.setRemoteKinds(QStringLiteral("webcam"), Device::NodeKind::TextureIn);
    plug.setRemoteKinds(
        QStringLiteral("keyboard"),
        Device::NodeKind::MidiIn | Device::NodeKind::MidiOut);

    QWidget parent;
    // The port decides which kind is wanted: an inlet reads what a device
    // produces, an outlet feeds what it consumes.
    Process::MidiInlet in{QStringLiteral("in"), Id<Process::Port>{0}, &parent};
    Process::MidiOutlet out{QStringLiteral("out"), Id<Process::Port>{1}, &parent};

    auto* midiIn = Process::makePortAddressCombo(in, doc->context(), &parent);
    auto* midiOut = Process::makePortAddressCombo(out, doc->context(), &parent);
    REQUIRE(midiIn);
    REQUIRE(midiOut);

    // A device can be several things at once, and both directions list it.
    CHECK(has(*midiIn, QStringLiteral("keyboard")));
    CHECK(has(*midiOut, QStringLiteral("keyboard")));

    // Each kind offers only its own: a window is not somewhere to read MIDI
    // from, and offering it would produce a port that cannot bind.
    CHECK_FALSE(has(*midiIn, QStringLiteral("stagewindow")));
    CHECK_FALSE(has(*midiIn, QStringLiteral("webcam")));
    CHECK_FALSE(has(*midiOut, QStringLiteral("stagewindow")));
  });
}

TEST_CASE("A device reported after the combo was built still appears", "[terminal]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    const auto bytes = emptyDocument(ctx);
    auto* doc = reload(ctx, bytes, score::DocumentRole::Terminal);
    REQUIRE(doc);
    auto& plug = doc->context().plugin<Explorer::DeviceDocumentPlugin>();

    QWidget parent;
    Process::MidiInlet port{QStringLiteral("in"), Id<Process::Port>{0}, &parent};
    auto* box = Process::makePortAddressCombo(port, doc->context(), &parent);
    REQUIRE(box);
    REQUIRE_FALSE(has(*box, QStringLiteral("keyboard")));

    // Plugged in on the other machine while this inspector was open.
    plug.setRemoteKinds(QStringLiteral("keyboard"), Device::NodeKind::MidiIn);
    QApplication::processEvents();

    CHECK(has(*box, QStringLiteral("keyboard")));
  });
}
