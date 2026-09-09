// The PipeWire video input device's settings panel.
//
// PipeWire's whole premise is that you pick a node the daemon is publishing
// right now. The panel asked the user to type its name into a line edit
// instead, which is neither discoverable nor checkable: a typo is a device
// that silently connects to nothing.
//
// The field is a combo box over the live Video/Source nodes now, still
// editable so a producer that is not up yet can be named by hand.

#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <Gfx/SharedInputSettings.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>

namespace
{
const auto pipewire_input_uuid
    = QStringLiteral("cf6a355f-34d1-4d24-a6ea-3d204f93cde9");

Device::ProtocolFactory* factory(const score::GUIApplicationContext& ctx)
{
  const auto key
      = UuidKey<Device::ProtocolFactory>::fromString(pipewire_input_uuid);
  return ctx.interfaces<Device::ProtocolFactoryList>().get(key);
}

//! The widget in the field cell of the row labelled `label`.
template <typename T>
T* fieldAs(QWidget& root, const QString& label)
{
  for(auto* form : root.findChildren<QFormLayout*>())
    for(int row = 0; row < form->rowCount(); ++row)
    {
      auto* li = form->itemAt(row, QFormLayout::LabelRole);
      auto* lw = li ? qobject_cast<QLabel*>(li->widget()) : nullptr;
      if(!lw || lw->text() != label)
        continue;
      auto* fi = form->itemAt(row, QFormLayout::FieldRole);
      if(fi)
        if(auto* w = qobject_cast<T*>(fi->widget()))
          return w;
    }
  return nullptr;
}

QString pathOf(const Device::DeviceSettings& s)
{
  return s.deviceSpecificSettings.value<Gfx::SharedInputSettings>().path;
}
}

TEST_CASE("the PipeWire input panel offers the nodes the daemon publishes",
          "[pipewire][settings]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* fact = factory(ctx);
    if(!fact)
      return; // built without PipeWire

    std::unique_ptr<Device::ProtocolSettingsWidget> w{fact->makeSettingsWidget()};
    REQUIRE(w);

    auto* node = fieldAs<QComboBox>(*w, QStringLiteral("PipeWire Node:"));
    REQUIRE(node != nullptr);

    // A picker, not a bare text field -- and still editable, for a producer
    // that is not up yet.
    CHECK(node->isEditable());
    CHECK(node->count() >= 1);

    // The first entry is "auto-connect": an empty node in the URL.
    node->setCurrentIndex(0);
    CHECK(node->itemData(0).toString().isEmpty());
    CHECK(pathOf(w->getSettings()).startsWith("pipewire://?"));
  });
}

TEST_CASE("a hand-typed PipeWire node reaches the URL", "[pipewire][settings]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* fact = factory(ctx);
    if(!fact)
      return;

    std::unique_ptr<Device::ProtocolSettingsWidget> w{fact->makeSettingsWidget()};
    REQUIRE(w);
    auto* node = fieldAs<QComboBox>(*w, QStringLiteral("PipeWire Node:"));
    REQUIRE(node != nullptr);

    node->setCurrentText(QStringLiteral("some-producer-not-running"));
    CHECK(pathOf(w->getSettings())
              .startsWith("pipewire://some-producer-not-running?"));
  });
}

TEST_CASE("the PipeWire input panel round-trips a saved node",
          "[pipewire][settings]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* fact = factory(ctx);
    if(!fact)
      return;

    std::unique_ptr<Device::ProtocolSettingsWidget> w{fact->makeSettingsWidget()};
    REQUIRE(w);

    Device::DeviceSettings in;
    in.name = "PipeWire";
    in.protocol = fact->concreteKey();
    Gfx::SharedInputSettings sis;
    sis.path = "pipewire://saved-node?width=640&height=480&fps=25&format=bgra";
    in.deviceSpecificSettings = QVariant::fromValue(sis);

    w->setSettings(in);
    const auto out = pathOf(w->getSettings());
    INFO(out.toStdString());
    CHECK(out.startsWith("pipewire://saved-node?"));
    CHECK(out.contains("width=640"));
    CHECK(out.contains("height=480"));
    CHECK(out.contains("format=bgra"));
  });
}
