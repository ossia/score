#include <Vst/Loader.hpp>

#include <ossia/detail/fmt.hpp>
#include <ossia/network/sockets/websocket.hpp>

#include <filesystem>
#include <iostream>
#include <set>

#if defined(_MSC_VER)
#include <boost/asio/impl/src.hpp>
#endif

// https://svn.boost.org/trac10/ticket/3605
#if defined(_MSC_VER)
#include <boost/asio/detail/winsock_init.hpp>
#pragma warning(push)
#pragma warning(disable : 4073)
#pragma init_seg(lib)
boost::asio::detail::winsock_init<2, 2>::manual manual_winsock_init;
#pragma warning(pop)
#elif defined(_WIN32)
#include <boost/asio/detail/winsock_init.hpp>
#endif

#if !defined(__cpp_exceptions)
#include <boost/throw_exception.hpp>
namespace boost
{
void throw_exception(std::exception const& e)
{
  std::terminate();
}
void throw_exception(std::exception const& e, boost::source_location const& loc)
{
  std::terminate();
}
}
#endif

intptr_t vst_host_callback(
    AEffect* effect, int32_t opcode, int32_t index, intptr_t value, void* ptr, float opt)
{
  intptr_t result = 0;

  switch(opcode)
  {
    case audioMasterGetTime: {
      static VstTimeInfo time;
      time.samplePos = 0.;
      time.sampleRate = 44100.;
      time.nanoSeconds = 0.;
      time.ppqPos = 0.;
      time.tempo = 120.;
      time.barStartPos = 0.;
      time.cycleStartPos = 0.;
      time.cycleEndPos = 0.;
      time.timeSigNumerator = 4;
      time.timeSigDenominator = 4;
      time.smpteOffset = 0;
      time.smpteFrameRate = 0;
      time.samplesToNextClock = 512;
      time.flags = kVstNanosValid | kVstPpqPosValid | kVstTempoValid | kVstBarsValid
                   | kVstTimeSigValid | kVstClockValid;
      result = reinterpret_cast<intptr_t>(&time);
      break;
    }
    case audioMasterSizeWindow:
      result = 1;
      break;
    case audioMasterNeedIdle:
      break;
    case audioMasterIdle:
      break;
    case audioMasterCurrentId:
      result = effect->uniqueID;
      break;
    case audioMasterUpdateDisplay:
      break;
    case audioMasterAutomate:
      break;
    case audioMasterProcessEvents:
      break;
    case audioMasterIOChanged:
      break;
    case audioMasterGetInputLatency:
      break;
    case audioMasterGetOutputLatency:
      break;
    case audioMasterVersion:
      result = kVstVersion;
      break;
    case audioMasterGetSampleRate:
      result = 44100;
      break;
    case audioMasterGetBlockSize:
      result = 512;
      break;
    case audioMasterGetCurrentProcessLevel:
      result = kVstProcessLevelUser;
      break;
    case audioMasterGetAutomationState:
      result = kVstAutomationUnsupported;
      break;
    case audioMasterGetLanguage:
      result = kVstLangEnglish;
      break;
    case audioMasterGetVendorVersion:
      result = 1;
      break;
    case audioMasterGetVendorString:
      std::copy_n("ossia", 6, static_cast<char*>(ptr));
      result = 1;
      break;
    case audioMasterGetProductString:
      std::copy_n("score", 6, static_cast<char*>(ptr));
      result = 1;
      break;
    case audioMasterBeginEdit:
      break;
    case audioMasterEndEdit:
      break;
    case audioMasterOpenFileSelector:
      break;
    case audioMasterCloseFileSelector:
      break;
    case audioMasterCanDo: {
      static const std::set<std::string_view> supported{
          HostCanDos::canDoSendVstEvents,
          HostCanDos::canDoSendVstMidiEvent,
          HostCanDos::canDoSendVstTimeInfo,
          HostCanDos::canDoSendVstMidiEventFlagIsRealtime,
          HostCanDos::canDoSizeWindow,
          HostCanDos::canDoHasCockosViewAsConfig};
      if(supported.find(static_cast<const char*>(ptr)) != supported.end())
        result = 1;
      break;
    }
  }
  return result;
}

static std::string getString(AEffect* fx, AEffectOpcodes op, int param)
{
  char paramName[512] = {0};
  fx->dispatcher(fx, op, param, 0, paramName, 0.f);
  return paramName;
}

std::string load_vst(const std::string& path, int id)
{
  try
  {
    const bool isFile = std::filesystem::exists(path);
    if(!isFile)
    {
      std::cerr << "Invalid path: " << path << std::endl;
      return {};
    }

    vst::Module plugin{path};

    if(auto m = plugin.getMain())
    {
      if(auto p = (AEffect*)m(vst_host_callback))
      {
        auto str = fmt::format(
            R"_({{
"UniqueID":{},
"Controls":{},
"Author":"{}",
"PrettyName":"{}",
"Version":"{}",
"Synth":{},
"Path":"{}",
"Request":{}
}})_",
            p->uniqueID, p->numParams, getString(p, effGetVendorString, 0),
            getString(p, effGetProductString, 0), getString(p, effGetVendorVersion, 0),
            bool(p->flags & effFlagsIsSynth), path, id);

        p->dispatcher(p, AEffectOpcodes::effClose, 0, 0, nullptr, 0.f);
        return str;
      }
    }
  }
  catch(const std::runtime_error& e)
  {
    std::cerr << e.what() << std::endl;
  }
  return {};
}

struct app
{

  boost::asio::io_context ctx;
  ossia::net::websocket_simple_client socket{{.url = "ws://127.0.0.1:37587"}, ctx};

  bool socket_ready{}, vst_ready{};
  std::string json_ret;

  app()
  {
    socket.on_open.connect<&app::on_open>(*this);
    socket.on_fail.connect<&app::on_error>(*this);
    socket.on_close.connect<&app::on_error>(*this);

    socket.websocket_client::connect("ws://127.0.0.1:37587");
  }

  void on_ready()
  {
    if(socket_ready && vst_ready)
    {
      socket.send_message(json_ret);
      socket.close();
      boost::asio::post(ctx, [&] { exit(json_ret.empty() ? 1 : 0); });
    }
  }

  void on_error()
  {
    std::cerr << "Socket error\n";
    exit(1);
  }

  void load(const std::string& vst, int id)
  {
    json_ret = load_vst(vst, id);
    std::cout << json_ret << "\n";
    vst_ready = true;
    on_ready();
  }

  void on_open()
  {
    socket_ready = true;
    on_ready();
  }
};

void init_invisible_window();
int main(int argc, char** argv)
{
  if(argc <= 1)
    return 1;

  init_invisible_window();

  int id = 0;
  if(argc > 2)
    id = std::atoi(argv[2]);

  app a;

  boost::asio::post(a.ctx, [&] { a.load(argv[1], id); });

  boost::asio::steady_timer tm{a.ctx};
  tm.expires_after(std::chrono::seconds(10));
  tm.async_wait([](auto ec) {
    std::cerr << "Timeout\n";
    exit(1);
  });

  a.ctx.run();
  a.ctx.restart();
  a.ctx.run();
  return a.json_ret.empty() ? 1 : 0;
}
