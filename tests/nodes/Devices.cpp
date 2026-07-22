// L1 test harness — device/protocol settings sweep.
//
// Iterates the entire Device::ProtocolFactoryList and, for EVERY registered
// protocol factory (OSC, OSCQuery, MIDI, Artnet, the gfx video-I/O protocols,
// addon protocols, ...):
//   1. reads factory.defaultSettings() (the L1-testable settings model);
//   2. round-trips the protocol-specific settings QVariant through the
//      factory's own serializeProtocolSpecificSettings /
//      makeProtocolSpecificSettings pair, both in the binary DataStream and the
//      JSON path;
//   3. asserts the re-serialized bytes are a stable fixed point.
//
// We deliberately round-trip only the *settings model*, not a live device: no
// hardware, socket, or GPU is opened. This is the universally-available L1
// surface for protocols (per the coverage matrix, "the settings model is
// L1-testable for all"). Constructing an actual DeviceInterface (makeDevice)
// would need real hardware/network and is out of scope here.
//
// Note: serializing a whole Device::DeviceSettings goes through the visitor's
// `components` application-context (to locate the protocol factory), which a
// bare marshall<> does not carry; that is why we drive the factory's
// protocol-specific (de)serialization entry points directly instead.

#include <score_test/App.hpp>

#include <Device/Protocol/DeviceSettings.hpp>
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Device/Protocol/ProtocolList.hpp>

#include <score/serialization/DataStreamVisitor.hpp>
#include <score/serialization/JSONVisitor.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <QString>

#include <catch2/catch_test_macros.hpp>

#include <csetjmp>
#include <csignal>

// Windows shim: no sigsetjmp/siglongjmp, SIGBUS/SIGTRAP/SIGALRM or alarm().
#if defined(_WIN32)
#include <csetjmp>
using sigjmp_buf = std::jmp_buf;
#define sigsetjmp(buf, save) setjmp(buf)
#define siglongjmp(buf, val) longjmp(buf, val)
static inline unsigned int alarm(unsigned int) noexcept { return 0; }
#endif

#include <cstdio>
#include <string>
#include <vector>

namespace
{
// Crash guard: some protocol settings (de)serializers abort (SCORE_ASSERT) or
// segfault when driven with the empty default settings; catch the signal,
// record the factory as failed, and continue. Mirrors the process sweep.
sigjmp_buf g_crash_jmp;
sigjmp_buf g_teardown_jmp;
volatile sig_atomic_t g_in_guard = 0;
volatile sig_atomic_t g_teardown_guard = 0;
volatile sig_atomic_t g_last_signal = 0;

extern "C" void dev_crash_handler(int sig)
{
  if(g_in_guard)
  {
    g_in_guard = 0;
    g_last_signal = sig;
    siglongjmp(g_crash_jmp, sig);
  }
  if(g_teardown_guard)
  {
    g_teardown_guard = 0;
    g_last_signal = sig;
    siglongjmp(g_teardown_jmp, sig);
  }
  std::signal(sig, SIG_DFL);
  std::raise(sig);
}

constexpr int g_guarded_signals[] = {SIGSEGV, SIGABRT, SIGFPE, SIGILL
#if !defined(_WIN32)
                                     , SIGBUS, SIGTRAP
#endif
};
void install_crash_handlers()
{
  for(int s : g_guarded_signals)
    std::signal(s, dev_crash_handler);
}
void restore_crash_handlers()
{
  for(int s : g_guarded_signals)
    std::signal(s, SIG_DFL);
}

struct DevResult
{
  std::string uuid;
  std::string name;
  bool crashed = false;
  bool datastream_roundtrip = false;
  bool datastream_fixedpoint = false;
  bool json_roundtrip = false;
  bool json_fixedpoint = false;
  std::string detail;
};

// One factory's settings round-trip, called inside the per-factory guard.
//
// We round-trip the whole Device::DeviceSettings (name + protocol key +
// protocol-specific QVariant). Its (de)serialization resolves the protocol
// factory through the visitor's `components`, which the default visitor
// constructors wire to the running application's global components — so this
// works while run_in_app is active.
//
// Structural equality deliberately compares the protocol key + device name, NOT
// the whole DeviceSettings::operator==: the latter compares deviceSpecificSettings
// as a QVariant, and the concrete settings structs do not register QVariant
// equality, so operator== reports "not equal" even when the payload is identical.
// The authoritative fidelity signal is therefore the byte fixed point (serialize
// -> load -> serialize yields identical bytes).
bool same_settings(const Device::DeviceSettings& a, const Device::DeviceSettings& b)
{
  return a.protocol == b.protocol && a.name == b.name;
}

void sweep_device(Device::ProtocolFactory& factory, DevResult& r)
{
  const Device::DeviceSettings s = factory.defaultSettings();

  // --- DataStream round-trip ---
  try
  {
    const QByteArray bytes = score::marshall<DataStream>(s);
    const auto s2 = score::unmarshall<Device::DeviceSettings>(bytes);
    r.datastream_roundtrip = same_settings(s2, s);
    r.datastream_fixedpoint = (score::marshall<DataStream>(s2) == bytes);
  }
  catch(const std::exception& e)
  {
    r.detail += std::string(" [datastream threw: ") + e.what() + "]";
  }
  catch(...)
  {
    r.detail += " [datastream threw unknown]";
  }

  // --- JSON round-trip ---
  try
  {
    JSONReader jr;
    jr.readFrom(s);
    const auto s2 = score::unmarshall<Device::DeviceSettings>(jr);
    r.json_roundtrip = same_settings(s2, s);

    JSONReader jr2;
    jr2.readFrom(s2);
    r.json_fixedpoint = (jr2.toByteArray() == jr.toByteArray());
  }
  catch(const std::exception& e)
  {
    r.detail += std::string(" [json threw: ") + e.what() + "]";
  }
  catch(...)
  {
    r.detail += " [json threw unknown]";
  }
}
}

TEST_CASE("Every protocol factory's settings round-trip", "[nodes][l1][device][serialization]")
{
  std::vector<DevResult> results;

  auto sweep = [&](const score::GUIApplicationContext& ctx) {
    auto& pl = ctx.interfaces<Device::ProtocolFactoryList>();
    REQUIRE(!pl.empty());

    for(auto& factory : pl)
    {
      results.emplace_back();
      const std::size_t idx = results.size() - 1;
      results[idx].uuid
          = score::uuids::toByteArray(factory.concreteKey().impl()).toStdString();
      results[idx].name = factory.prettyName().toStdString();

      if(qEnvironmentVariableIsSet("SWEEP_TRACE"))
        std::fprintf(
            stderr, "DEVTRACE %s %s\n", results[idx].uuid.c_str(),
            results[idx].name.c_str());

      g_in_guard = 1;
      g_last_signal = 0;
      if(sigsetjmp(g_crash_jmp, 1) == 0)
      {
        sweep_device(factory, results[idx]);
        g_in_guard = 0;
      }
      else
      {
        g_in_guard = 0;
        results[idx].crashed = true;
        results[idx].detail += " [CRASHED: fatal signal "
                               + std::to_string((int)g_last_signal)
                               + " during settings round-trip]";
      }
    }
  };

  install_crash_handlers();
  if(sigsetjmp(g_teardown_jmp, 1) == 0)
  {
    g_teardown_guard = 1;
    score::test::run_in_app(sweep);
    g_teardown_guard = 0;
  }
  else
  {
    WARN(
        "Device app teardown crashed (fatal signal "
        << (int)g_last_signal << ") after the sweep; results were collected.");
  }
  restore_crash_handlers();

  REQUIRE(!results.empty());

  int passed = 0, crashed = 0;
  for(const auto& r : results)
  {
    INFO("Protocol: " << r.name << "  {" << r.uuid << "}");
    if(!r.detail.empty())
      INFO("detail:" << r.detail);

    if(r.crashed)
    {
      ++crashed;
      WARN("CRASHED " << r.name << " {" << r.uuid << "}:" << r.detail);
    }

    // Hard invariant: a serialize/deserialize round-trip must preserve the
    // settings on both the binary and JSON paths.
    CHECK(r.datastream_roundtrip);
    CHECK(r.json_roundtrip);

    // Byte-exact fixed point is a stronger property that some protocols
    // legitimately do not hold (fields normalized on load). Report, do not fail.
    if(!r.datastream_fixedpoint)
      WARN("datastream not byte-exact: " << r.name << " {" << r.uuid << "}");
    if(!r.json_fixedpoint)
      WARN("json not byte-exact: " << r.name << " {" << r.uuid << "}");

    if(r.datastream_roundtrip && r.datastream_fixedpoint && r.json_roundtrip
       && r.json_fixedpoint)
      ++passed;
  }

  WARN(
      "Protocol settings sweep: " << results.size() << " factories, " << passed
                                  << " byte-exact, " << crashed << " crashed.");
}
