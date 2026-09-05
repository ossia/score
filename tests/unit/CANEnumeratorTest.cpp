// The CAN protocol's device browser entry and its default interface.
//
// The browser lists the .dbc databases of the user library, as the OSC / Serial
// / WS protocols list their own file types. It used to list the machine's CAN
// interfaces, which the settings widget's combo already shows.
//
// defaultSettings() used to hardcode "can0", so a fresh device on a machine
// without a physical CAN port failed with "no such CAN interface". Both are
// checked against an independent walk of /sys/class/net done here, so a bug in
// the implementation's own helper cannot make the test agree with it.

#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <Library/LibrarySettings.hpp>
#include <Protocols/CAN/CANSpecificSettings.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>

#include <catch2/catch_all.hpp>
#include <score_test/App.hpp>
#include <score_test/Document.hpp>

#include <algorithm>
#include <memory>

// Everything below needs Protocols::CANSpecificSettings, which its header only
// declares under OSSIA_PROTOCOL_CAN -- the same condition the CMake target
// applies, and the one that actually governs whether the type exists.
#if defined(OSSIA_PROTOCOL_CAN)
namespace
{
//! The uuid of Protocols::CANProtocolFactory. Spelled out rather than taken
//! from the class so that a renamed or moved factory still has to keep the key
//! its saved scores refer to.
constexpr UuidKey<Device::ProtocolFactory> can_protocol_key()
{
  return UuidKey<Device::ProtocolFactory>{"2492941c-18ee-4f96-ac3d-c3d42c0bb649"};
}

//! ARPHRD_CAN.
constexpr int arphrd_can = 280;

//! What the machine actually has, read straight out of sysfs.
QStringList sysfsCanInterfaces()
{
  QStringList out;
  QDir sys{"/sys/class/net"};
  for(const auto& name : sys.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System))
  {
    QFile type{"/sys/class/net/" + name + "/type"};
    if(!type.open(QIODevice::ReadOnly | QIODevice::Text))
      continue;

    bool ok = false;
    if(type.readAll().trimmed().toInt(&ok) == arphrd_can && ok)
      out.push_back(name);
  }
  out.sort();
  return out;
}

Device::ProtocolFactory* canFactory(const score::GUIApplicationContext& ctx)
{
  auto& list = ctx.interfaces<Device::ProtocolFactoryList>();
  return list.get(can_protocol_key());
}

//! A minimal but real database: one message, two signals.
constexpr auto sample_dbc = R"_(VERSION ""

BS_:

BU_: Sensor

BO_ 385 PDO1_Transmit: 4 Sensor
 SG_ AccelerationX : 0|16@1- (0.001,0) [-32|32] "g" Vector__XXX
 SG_ AccelerationY : 16|16@1- (0.001,0) [-32|32] "g" Vector__XXX
)_";

/**
 * Files dropped in the library for the duration of one test case.
 *
 * The test application's library root is
 * ~/Documents/ossia/score-test (applicationName is "score-test", not "score"),
 * so this never touches the real one.
 */
struct library_files
{
  QString dir;
  QStringList written;

  explicit library_files(const score::GUIApplicationContext& ctx)
  {
    dir = ctx.settings<Library::Settings::Model>().getPackagesPath()
          + "/can-enumerator-test";
    QDir{}.mkpath(dir);
  }

  //! The library lives under the documents directory, which a CI container may
  //! not have: nothing to test there rather than a failure.
  bool usable() const { return QFileInfo{dir}.isWritable(); }

  ~library_files() { QDir{dir}.removeRecursively(); }

  //! Returns the absolute path of the file it wrote.
  QString write(const QString& name, const QByteArray& content)
  {
    const QString path = dir + '/' + name;
    QFile f{path};
    if(!f.open(QIODevice::WriteOnly))
      FAIL("could not write " << path.toStdString());
    REQUIRE(f.write(content) == content.size());
    f.close();
    written.push_back(path);
    return QFileInfo{path}.absoluteFilePath();
  }
};

struct enumerated
{
  QString name;
  Device::DeviceSettings settings;
};

/**
 * Collects what an enumerator reports, until \a until shows up or the deadline
 * passes.
 *
 * LibraryDeviceEnumerator scans on a worker thread and reports through queued
 * signals, so there is nothing to read synchronously: enumerate() on it is a
 * no-op by design, and the only way to observe it is to run the event loop.
 *
 * Waiting for a named entry rather than for a fixed duration is what keeps this
 * honest in both directions: a test that asserts a file is *absent* has to know
 * the scan actually ran, and the file that must be there is the proof of it.
 */
std::vector<enumerated> collect(
    Device::DeviceEnumerator& e, const QString& until,
    std::chrono::seconds deadline = std::chrono::seconds(20))
{
  std::vector<enumerated> out;
  QObject::connect(
      &e, &Device::DeviceEnumerator::deviceAdded, qApp,
      [&out](const QString& n, const Device::DeviceSettings& s) {
    out.push_back({n, s});
  });

  const auto seen = [&out, &until] {
    return std::any_of(out.begin(), out.end(), [&until](const enumerated& e) {
      return e.name == until;
    });
  };

  QElapsedTimer t;
  t.start();
  while(!seen() && t.elapsed() < deadline.count() * 1000)
    QApplication::processEvents(QEventLoop::AllEvents, 20);

  // The commit actions are delivered in batches of up to 255; drain whatever
  // else is on its way, so that "this file was not offered" means it, rather
  // than meaning "it was in the next batch".
  const auto drain = t.elapsed() + 250;
  while(t.elapsed() < drain)
    QApplication::processEvents(QEventLoop::AllEvents, 20);

  return out;
}

const enumerated* find(const std::vector<enumerated>& v, const QString& name)
{
  auto it = std::find_if(
      v.begin(), v.end(), [&name](const enumerated& e) { return e.name == name; });
  return it == v.end() ? nullptr : &*it;
}

Protocols::CANSpecificSettings specific(const Device::DeviceSettings& s)
{
  return s.deviceSpecificSettings.value<Protocols::CANSpecificSettings>();
}
}


TEST_CASE("the CAN protocol offers the databases in the library", "[can]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* factory = canFactory(ctx);
    if(!factory)
    {
      SUCCEED("this build has no CAN protocol");
      return;
    }

    library_files lib{ctx};
    if(!lib.usable())
      SKIP("no writable user library at " << lib.dir.toStdString());
    const auto dbc = lib.write("LPMS3_sample.dbc", sample_dbc);

    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto enumerators = factory->getEnumerators(doc->context());
    REQUIRE(enumerators.size() == 1);
    REQUIRE(enumerators[0].second != nullptr);
    std::unique_ptr<Device::DeviceEnumerator> owned{enumerators[0].second};

    const auto found = collect(*owned, "LPMS3_sample");

    // Named after the file, so that a library of a dozen vendor databases is
    // navigable.
    const auto* e = find(found, "LPMS3_sample");
    REQUIRE(e != nullptr);
    REQUIRE(e->settings.protocol == can_protocol_key());

    SECTION("the database is filled in, so the device arrives with its tree")
    {
      const auto specif = specific(e->settings);
      REQUIRE(specif.dbcPath == dbc);
      REQUIRE(QFile::exists(specif.dbcPath));
    }

    SECTION("and so is the interface, so it is connectable as-is")
    {
      // Only assertable when the machine has one at all; on a machine with no
      // CAN interface the field is legitimately empty and the user must pick.
      const auto present = sysfsCanInterfaces();
      const auto specif = specific(e->settings);
      if(present.empty())
        REQUIRE(specif.interfaceName.isEmpty());
      else
        REQUIRE(present.contains(specif.interfaceName));
    }

    SECTION("the defaults of the settings are left alone")
    {
      const auto specif = specific(e->settings);
      // float32Override contradicts what a database says and must stay opt-in
      // even for a file the browser offered.
      REQUIRE(!specif.float32Override);
      REQUIRE(specif.nodeIdOffset == 0);
    }
  });
}

TEST_CASE("the CAN protocol offers only actual databases", "[can]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* factory = canFactory(ctx);
    if(!factory)
    {
      SUCCEED("this build has no CAN protocol");
      return;
    }

    library_files lib{ctx};
    if(!lib.usable())
      SKIP("no writable user library at " << lib.dir.toStdString());
    lib.write("real.dbc", sample_dbc);

    // Right extension, but no message in it: nothing for a device to read.
    lib.write("empty.dbc", "VERSION \"\"\n\nBS_:\n\nBU_: Sensor\n");

    // The message keyword, but not a database: the extension is what says
    // "this is a CAN file", and matching content alone would drag in every
    // stray text file that happens to contain "BO_".
    lib.write("notes.txt", sample_dbc);

    auto doc = score::test::new_document(ctx);
    REQUIRE(doc);

    auto enumerators = factory->getEnumerators(doc->context());
    REQUIRE(enumerators.size() == 1);
    std::unique_ptr<Device::DeviceEnumerator> owned{enumerators[0].second};

    // "real" is the control: once it has arrived the scan has run, so the
    // absence of the other two is a statement about the filter, not about
    // timing.
    const auto found = collect(*owned, "real");

    REQUIRE(find(found, "real") != nullptr);
    REQUIRE(find(found, "empty") == nullptr);
    REQUIRE(find(found, "notes") == nullptr);
  });
}

TEST_CASE("the CAN default settings name an interface that exists", "[can]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* factory = canFactory(ctx);
    if(!factory)
    {
      SUCCEED("this build has no CAN protocol");
      return;
    }

    const auto present = sysfsCanInterfaces();
    const auto specif = specific(factory->defaultSettings());

    if(present.empty())
    {
      // Nothing rather than a name the kernel will reject.
      REQUIRE(specif.interfaceName.isEmpty());
    }
    else
    {
      REQUIRE(present.contains(specif.interfaceName));

      // A physical adapter wins over a vcan left over from a test.
      const bool anyPhysical
          = std::any_of(present.begin(), present.end(), [](const QString& n) {
        return !n.startsWith("vcan");
      });
      if(anyPhysical)
        REQUIRE(!specif.interfaceName.startsWith("vcan"));
    }
  });
}

TEST_CASE("the CAN default settings follow the machine, not a cached value", "[can]")
{
  score::test::run_in_app([](const score::GUIApplicationContext& ctx) {
    auto* factory = canFactory(ctx);
    if(!factory)
    {
      SUCCEED("this build has no CAN protocol");
      return;
    }

    // Recomputed on every call, so an adapter plugged in after score started
    // shows up; two consecutive calls must still agree.
    const auto a = factory->defaultSettings();
    const auto b = factory->defaultSettings();

    REQUIRE(specific(a).interfaceName == specific(b).interfaceName);
  });
}

#endif
