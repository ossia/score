#include <Vst/Loader.hpp>

#include <score/tools/PuppetClient.hpp>

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

std::string load_vst(const std::string& path, int id, const std::string& token)
{
  using score::puppet::json_escape;
  auto error_json = [&](std::string err) {
    return fmt::format(
        R"_({{"Path":"{}","Request":{},"Token":"{}","Error":"{}"}})_",
        json_escape(path), id, json_escape(token), json_escape(err));
  };

  try
  {
    const bool isFile = std::filesystem::exists(path);
    if(!isFile)
    {
      std::cerr << "Invalid path: " << path << std::endl;
      return error_json("Invalid path");
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
"Request":{},
"Token":"{}"
}})_",
            p->uniqueID, p->numParams, json_escape(getString(p, effGetVendorString, 0)),
            json_escape(getString(p, effGetProductString, 0)),
            json_escape(getString(p, effGetVendorVersion, 0)),
            bool(p->flags & effFlagsIsSynth), json_escape(path), id,
            json_escape(token));

        p->dispatcher(p, AEffectOpcodes::effClose, 0, 0, nullptr, 0.f);
        return str;
      }
    }
    return error_json("No VST entry point");
  }
  catch(const std::runtime_error& e)
  {
    std::cerr << e.what() << std::endl;
    return error_json(e.what());
  }
  catch(...)
  {
    return error_json("Unknown exception");
  }
}

void init_invisible_window();
int main(int argc, char** argv)
{
  init_invisible_window();

  return score::puppet::puppet_main(
      argc, argv, 37587, "vstpuppet", true, 30,
      [](const std::string& path, int id, const std::string& token) {
    return load_vst(path, id, token);
      });
}

#include <score/tools/WinMainToMain.hpp>
