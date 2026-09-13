// The MIDI Controller's settings dialog at application level: choosing a
// description in the picker has to fill the preview beside it, which is the
// only thing that tells the user whether it has the controls they want, and
// has to be enough on its own to produce settings.
//
// Driven through the picker's selection model rather than through
// setSettings(), because the two reach the preview by different paths.

#include <Library/LibrarySettings.hpp>
#include <Protocols/MCU/MCUProtocolFactory.hpp>
#include <Protocols/MCU/MCUSpecificSettings.hpp>
#include <Protocols/MIDIDevices/MidiDeviceDatabase.hpp>

#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <score_test/App.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <QDir>
#include <QFile>
#include <QItemSelectionModel>
#include <QTreeView>
#include <QTreeWidget>
#include <catch2/catch_all.hpp>

#include <memory>

namespace
{
constexpr auto fixture = R"_({
  "format": "score.midi-device/1",
  "manufacturer": "Test",
  "model": "Two Knobs",
  "controls": [
    {
      "name": "Volume",
      "kind": "knob",
      "group": ["Mixer"],
      "direction": "in",
      "message": { "type": "cc", "channel": 1, "number": 7 }
    },
    {
      "name": "Pan",
      "kind": "knob",
      "group": ["Mixer"],
      "direction": "in",
      "message": { "type": "cc", "channel": 1, "number": 10 }
    }
  ]
})_";

/**
 * Point the library at a package of our own so the picker has exactly one
 * description to choose: the developer's installed maps are not a fixture, and
 * without any the test would pass by having nothing to do.
 */
QString installFixture()
{
  const QString root = QDir::tempPath() + "/score-tests/midi-device-picker";
  QDir{root}.removeRecursively();

  const QString dir = root + "/packages/midi-device-maps/maps/test";
  REQUIRE(QDir{}.mkpath(dir));

  QFile f{dir + "/two-knobs.midimap.json"};
  REQUIRE(f.open(QIODevice::WriteOnly));
  f.write(fixture);
  f.close();
  return root;
}

//! The first row that names a description; the manufacturer rows do not.
QModelIndex firstDevice(QTreeView& picker)
{
  auto* model = picker.model();
  for(int m = 0; m < model->rowCount(); m++)
  {
    const auto group = model->index(m, 0);
    if(model->rowCount(group) > 0)
      return model->index(0, 0, group);
  }
  return {};
}
}

TEST_CASE("choosing a description fills the preview", "[mididevice][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    ctx.settings<Library::Settings::Model>().setRootPath(installFixture());

    auto& db = Protocols::MIDIDevices::Database::instance();
    db.rescan();
    REQUIRE(db.devices().size() == 1);

    auto* factory = ctx.interfaces<Device::ProtocolFactoryList>().get(
        Protocols::MCUProtocolFactory::static_concreteKey());
    REQUIRE(factory);

    std::unique_ptr<Device::ProtocolSettingsWidget> widget{
        factory->makeSettingsWidget()};
    REQUIRE(widget);

    auto* picker = widget->findChild<QTreeView*>("picker");
    auto* preview = widget->findChild<QTreeWidget*>("preview");
    REQUIRE(picker);
    REQUIRE(preview);

    // Nothing chosen yet: the preview has nothing to show and must not pretend.
    CHECK(preview->topLevelItemCount() == 0);

    const auto idx = firstDevice(*picker);
    REQUIRE(idx.isValid());

    picker->selectionModel()->setCurrentIndex(
        idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    // The description's own groups and controls, without the level naming the
    // device: this previews one description, not the assembled tree.
    REQUIRE(preview->topLevelItemCount() == 1);
    auto* group = preview->topLevelItem(0);
    CHECK(group->text(0) == "Mixer");
    CHECK(group->childCount() == 2);

    // Picking is enough: adding to the list is only needed for a chain.
    const auto settings = widget->getSettings();
    const auto midi
        = settings.deviceSpecificSettings.value<Protocols::MCUSpecificSettings>();
    CHECK(midi.mode == Protocols::MCUSpecificSettings::MidiDeviceMap);
    REQUIRE(midi.maps.size() == 1);
    CHECK(midi.maps[0].map == "test/two-knobs.midimap.json");
    CHECK(midi.maps[0].channel == 1);
  });
}
