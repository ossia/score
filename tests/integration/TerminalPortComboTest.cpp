// A terminal has no device objects, so the port inspector cannot find out what
// a device can be plugged into by casting it. It uses the kinds the machine
// running the score reported instead.

#include <Device/Protocol/DeviceInterface.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <Process/Dataflow/AudioPortComboBox.hpp>
#include <Process/Dataflow/Port.hpp>

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

bool has(const QComboBox& box, const QString& name)
{
  const auto e = entries(box);
  return std::find(e.begin(), e.end(), name) != e.end();
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

    plug.setRemoteKinds(QStringLiteral("stagewindow"), Device::DeviceKind::TextureOut);
    plug.setRemoteKinds(QStringLiteral("webcam"), Device::DeviceKind::TextureIn);
    plug.setRemoteKinds(
        QStringLiteral("keyboard"),
        Device::DeviceKinds{Device::DeviceKind::MidiIn} | Device::DeviceKind::MidiOut);

    QWidget parent;
    Process::ValueInlet port{QStringLiteral("in"), Id<Process::Port>{0}, &parent};

    auto* out = Process::makeDeviceCombo(
        Device::DeviceKind::TextureOut, plug.list(), port, doc->context(), &parent);
    REQUIRE(out);
    CHECK(has(*out, QStringLiteral("stagewindow")));

    // Each kind offers only its own: a window is not somewhere to read a
    // texture from, and offering it would produce a port that cannot bind.
    CHECK_FALSE(has(*out, QStringLiteral("webcam")));
    CHECK_FALSE(has(*out, QStringLiteral("keyboard")));

    auto* in = Process::makeDeviceCombo(
        Device::DeviceKind::TextureIn, plug.list(), port, doc->context(), &parent);
    REQUIRE(in);
    CHECK(has(*in, QStringLiteral("webcam")));
    CHECK_FALSE(has(*in, QStringLiteral("stagewindow")));

    // A device can be several things at once, and both directions must list it.
    auto* midiIn = Process::makeDeviceCombo(
        Device::DeviceKind::MidiIn, plug.list(), port, doc->context(), &parent);
    auto* midiOut = Process::makeDeviceCombo(
        Device::DeviceKind::MidiOut, plug.list(), port, doc->context(), &parent);
    REQUIRE(midiIn);
    REQUIRE(midiOut);
    CHECK(has(*midiIn, QStringLiteral("keyboard")));
    CHECK(has(*midiOut, QStringLiteral("keyboard")));
    CHECK_FALSE(has(*midiIn, QStringLiteral("stagewindow")));
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
    Process::ValueInlet port{QStringLiteral("in"), Id<Process::Port>{0}, &parent};
    auto* box = Process::makeDeviceCombo(
        Device::DeviceKind::TextureOut, plug.list(), port, doc->context(), &parent);
    REQUIRE(box);
    REQUIRE_FALSE(has(*box, QStringLiteral("stagewindow")));

    // Plugged in on the other machine while this inspector was open.
    plug.setRemoteKinds(QStringLiteral("stagewindow"), Device::DeviceKind::TextureOut);
    QApplication::processEvents();

    CHECK(has(*box, QStringLiteral("stagewindow")));
  });
}
