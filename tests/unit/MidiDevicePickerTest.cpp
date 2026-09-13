// The MIDI Controller's settings dialog at application level.
//
// Driven through the picker's selection model, not setSettings(): the two
// reach the preview by different paths.

#include <Library/LibrarySettings.hpp>
#include <Protocols/MCU/MCUProtocolFactory.hpp>
#include <Protocols/MCU/MCUSpecificSettings.hpp>
#include <Protocols/MIDIDevices/MidiDeviceDatabase.hpp>

#include <Device/Protocol/ProtocolList.hpp>
#include <Device/Protocol/ProtocolSettingsWidget.hpp>

#include <score_test/App.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <score/serialization/JSONVisitor.hpp>

#include <QDir>
#include <QFile>
#include <QPushButton>
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
 * Point the library at a package of our own: whatever maps are installed are
 * not a fixture, and with none the test would pass by having nothing to do.
 *
 * Two spellings of one brand, and a model that repeats it, as the corpus has.
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
    REQUIRE(model->rowCount() == 4);

    // Raw MIDI first: the one row that is not a brand.
    const auto generic = model->index(0, 0);
    CHECK(generic.data().toString() == "Generic");
    CHECK(
        childNames(*picker, generic)
        == QStringList{"MIDI channel", "MIDI channel, every note and control"});

    const auto korg = model->index(1, 0);
    CHECK(korg.data().toString() == "Korg");
    CHECK(model->index(2, 0).data().toString() == "Test");

    // A document that names no model is named by the file it lives in.
    const auto yamaha = model->index(3, 0);
    CHECK(yamaha.data().toString() == "Yamaha");
    CHECK(childNames(*picker, yamaha) == QStringList{"PLG100 XG Expansion"});

    // Sorted by model, none of them saying Korg twice.
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

    // Eight of one controller on one port is an ordinary rig: each opening
    // puts another on the next free channel.
    for(int i = 2; i <= 8; i++)
    {
      picker->doubleClicked(idx);
      REQUIRE(chosen->topLevelItemCount() == i);
      CHECK(chosen->topLevelItem(i - 1)->text(0) == "M1");
      CHECK(chosen->topLevelItem(i - 1)->text(1) == QString::number(i));
    }

    // A cable carries sixteen channels and no more, and the button that would
    // add a seventeenth says so rather than doing nothing.
    for(int i = 9; i <= 20; i++)
      picker->doubleClicked(idx);
    CHECK(chosen->topLevelItemCount() == 16);

    QPushButton* add{};
    for(auto* b : widget->findChildren<QPushButton*>())
      if(b->text() == QObject::tr("Add device"))
        add = b;
    REQUIRE(add);
    CHECK(!add->isEnabled());

    while(chosen->topLevelItemCount() > 1)
      delete chosen->topLevelItem(chosen->topLevelItemCount() - 1);

    // A raw channel goes on the port beside a description, and previews the
    // nodes it would build rather than nothing.
    const auto raw = rowNamed(*picker, generic, "MIDI channel, every note and control");
    REQUIRE(raw.isValid());

    picker->selectionModel()->setCurrentIndex(
        raw, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);

    REQUIRE(preview->topLevelItemCount() == 5);
    CHECK(preview->topLevelItem(0)->text(0) == "on");
    CHECK(preview->topLevelItem(0)->childCount() == 1);
    CHECK(preview->topLevelItem(4)->text(0) == "pitchbend");

    // Beside, not on top of: a channel carries one device, and the M1 already
    // has channel 1.
    picker->doubleClicked(raw);
    REQUIRE(chosen->topLevelItemCount() == 2);
    CHECK(chosen->topLevelItem(1)->text(0) == "MIDI channel, every note and control");
    CHECK(chosen->topLevelItem(1)->text(1) == "2");

    const auto both = widget->getSettings()
                          .deviceSpecificSettings.value<Protocols::MCUSpecificSettings>();
    REQUIRE(both.maps.size() == 2);
    CHECK(both.maps[1].map == "generic:channel+all");
  });
}

TEST_CASE("a document that names no mode is a Mackie Control", "[mididevice][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* factory = ctx.interfaces<Device::ProtocolFactoryList>().get(
        Protocols::MCUProtocolFactory::static_concreteKey());
    REQUIRE(factory);

    // Through the factory, which is the path a document load takes.
    const auto read = [&](const char* text) {
      rapidjson::Document doc;
      doc.Parse(text);
      REQUIRE(!doc.HasParseError());
      JSONWriter wrt{doc};
      return factory->makeProtocolSpecificSettings(wrt.toVariant())
          .value<Protocols::MCUSpecificSettings>();
    };

    // What a released score wrote: ports and an API, and nothing about a mode
    // or a device map, neither of which existed then.
    const auto old = read(R"_({"API": 0, "Input": [], "Output": []})_");
    CHECK(old.mode == Protocols::MCUSpecificSettings::MCU);
    CHECK(old.maps.empty());

    // And one that does name it keeps what it says.
    const auto mapped = read(R"_({
      "API": 0, "Input": [], "Output": [],
      "Mode": 1, "Maps": ["test/m1.midimap.json"], "Channels": [4]
    })_");
    CHECK(mapped.mode == Protocols::MCUSpecificSettings::MidiDeviceMap);
    REQUIRE(mapped.maps.size() == 1);
    CHECK(mapped.maps[0].map == "test/m1.midimap.json");
    CHECK(mapped.maps[0].channel == 4);
  });
}

TEST_CASE("taking the last device off the port leaves it empty", "[mididevice][gui]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    ctx.settings<Library::Settings::Model>().setRootPath(installFixture());
    Protocols::MIDIDevices::Database::instance().rescan();

    auto* factory = ctx.interfaces<Device::ProtocolFactoryList>().get(
        Protocols::MCUProtocolFactory::static_concreteKey());
    REQUIRE(factory);
    std::unique_ptr<Device::ProtocolSettingsWidget> widget{
        factory->makeSettingsWidget()};

    auto* picker = widget->findChild<QTreeView*>("picker");
    auto* chosen = widget->findChild<QTreeWidget*>("chosen");
    REQUIRE(picker);
    REQUIRE(chosen);

    auto* model = picker->model();
    QModelIndex korg;
    for(int r = 0; r < model->rowCount(); r++)
      if(model->index(r, 0).data().toString() == "Korg")
        korg = model->index(r, 0);
    REQUIRE(korg.isValid());

    const auto m1 = rowNamed(*picker, korg, "M1");
    const auto mini = rowNamed(*picker, korg, "minilogue");
    REQUIRE(m1.isValid());
    REQUIRE(mini.isValid());

    // A channel carries one device, whichever description it is: two different
    // maps must not both land on channel 1.
    picker->doubleClicked(m1);
    picker->doubleClicked(mini);
    REQUIRE(chosen->topLevelItemCount() == 2);
    CHECK(chosen->topLevelItem(0)->text(1) == "1");
    CHECK(chosen->topLevelItem(1)->text(1) == "2");

    // Through the button, which is the only way a user has of doing it.
    QPushButton* remove{};
    for(auto* b : widget->findChildren<QPushButton*>())
      if(b->text() == QObject::tr("Remove"))
        remove = b;
    REQUIRE(remove);

    const auto pressRemove = [&](int row) {
      chosen->setCurrentItem(chosen->topLevelItem(row));
      remove->click();
    };

    pressRemove(1);
    pressRemove(0);
    REQUIRE(chosen->topLevelItemCount() == 0);

    // An empty list is a port with nothing on it, not a list that was never
    // filled in: the device the user just took off must not come back.
    const auto midi = widget->getSettings()
                          .deviceSpecificSettings.value<Protocols::MCUSpecificSettings>();
    CHECK(midi.maps.empty());
  });
}
