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
#include <QStringList>
#include <QItemSelectionModel>
#include <QTreeView>
#include <QTreeWidget>
#include <catch2/catch_all.hpp>

#include <memory>

namespace
{
QByteArray map(const char* manufacturer, const char* model)
{
  return QStringLiteral(R"_({
  "format": "score.midi-device/1",
  "manufacturer": "%1",
  "model": "%2",
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
})_")
      .arg(QLatin1String{manufacturer}, QLatin1String{model})
      .toUtf8();
}

/**
 * Point the library at a package of our own: the developer's installed maps
 * are not a fixture, and without any the test would pass by having nothing to
 * do.
 *
 * Two spellings of one brand, and a model that repeats it, because that is
 * what the corpus is like -- 95 documents call themselves Korg, 30 of them
 * shouting.
 */
QString installFixture()
{
  const QString root = QDir::tempPath() + "/score-tests/midi-device-picker";
  QDir{root}.removeRecursively();

  const QString dir = root + "/packages/midi-device-maps/maps/test";
  REQUIRE(QDir{}.mkpath(dir));

  const auto write = [&dir](const QString& name, const QByteArray& text) {
    QFile f{dir + "/" + name + ".midimap.json"};
    REQUIRE(f.open(QIODevice::WriteOnly));
    f.write(text);
  };

  write("two-knobs", map("Test", "Two Knobs"));
  write("m1", map("Korg", "M1"));
  write("minilogue", map("Korg", "minilogue"));
  write("volca", map("KORG", "Korg Volca Keys"));

  // A MIDNAM that extends another document states "?" for the model.
  write("Yamaha_PLG100_XG_Expansion", map("Yamaha", "?"));
  return root;
}

//! The row of a group, by what it shows.
QModelIndex rowNamed(QTreeView& picker, const QModelIndex& group, const QString& name)
{
  auto* model = picker.model();
  for(int r = 0; r < model->rowCount(group); r++)
    if(const auto idx = model->index(r, 0, group); idx.data().toString() == name)
      return idx;
  return {};
}

QStringList childNames(QTreeView& picker, const QModelIndex& group)
{
  QStringList names;
  auto* model = picker.model();
  for(int r = 0; r < model->rowCount(group); r++)
    names << model->index(r, 0, group).data().toString();
  return names;
}
}

TEST_CASE("choosing a description fills the preview", "[mididevice][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    ctx.settings<Library::Settings::Model>().setRootPath(installFixture());

    auto& db = Protocols::MIDIDevices::Database::instance();
    db.rescan();
    REQUIRE(db.devices().size() == 5);

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

    // One brand, one group, however its documents spell it -- and spelled the
    // way most of them do.
    auto* model = picker->model();
    REQUIRE(model->rowCount() == 3);
    const auto korg = model->index(0, 0);
    CHECK(korg.data().toString() == "Korg");
    CHECK(model->index(1, 0).data().toString() == "Test");

    // A document that names no model is named by the file it lives in.
    const auto yamaha = model->index(2, 0);
    CHECK(yamaha.data().toString() == "Yamaha");
    CHECK(childNames(*picker, yamaha) == QStringList{"PLG100 XG Expansion"});

    // Sorted by model, and none of them says Korg twice.
    CHECK(childNames(*picker, korg) == QStringList{"Volca Keys", "M1", "minilogue"});

    // Nothing chosen yet: the preview has nothing to show and must not pretend.
    CHECK(preview->topLevelItemCount() == 0);

    const auto idx = rowNamed(*picker, korg, "M1");
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
    CHECK(midi.maps[0].map == "test/m1.midimap.json");
    CHECK(midi.maps[0].channel == 1);

    // Opening a row is how a device gets onto the port, and it goes there
    // called what the picker calls it rather than "Korg: M1".
    auto* chosen = widget->findChild<QTreeWidget*>("chosen");
    REQUIRE(chosen);
    REQUIRE(chosen->topLevelItemCount() == 0);

    picker->doubleClicked(idx);
    REQUIRE(chosen->topLevelItemCount() == 1);
    CHECK(chosen->topLevelItem(0)->text(0) == "M1");
    CHECK(chosen->topLevelItem(0)->text(1) == "1");

    // The same device twice would be two subtrees listening to one message.
    picker->doubleClicked(idx);
    CHECK(chosen->topLevelItemCount() == 1);
  });
}
