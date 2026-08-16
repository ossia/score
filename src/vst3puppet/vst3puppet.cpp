
#include <score/tools/PuppetClient.hpp>

#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>

#include <filesystem>
#include <iostream>

#include <public.sdk/source/vst/hosting/hostclasses.h>
#include <public.sdk/source/vst/hosting/module.h>
#include <public.sdk/source/vst/hosting/plugprovider.h>

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

using namespace Steinberg;
using score::puppet::json_escape;

static std::string
error_json(const std::string& path, int id, const std::string& token, std::string err)
{
  return fmt::format(
      R"_({{"Path":"{}","Request":{},"Token":"{}","Error":"{}"}})_", json_escape(path),
      id, json_escape(token), json_escape(err));
}

std::string load_vst(const std::string& path, int id, const std::string& token)
{
  try
  {
    const bool isFile = std::filesystem::exists(path);
    if(!isFile)
    {
      std::cerr << "Invalid path: " << path << std::endl;
      return error_json(path, id, token, "Invalid path");
    }

    std::string err;
    auto module = VST3::Hosting::Module::create(path, err);

    if(!module)
    {
      std::cerr << "Failed to load VST3 " << path << err << std::endl;
      return error_json(path, id, token, "Failed to load: " + err);
    }

    std::string root;

    std::vector<std::string> arr;
    const auto& fac = module->getFactory();
    const auto& fi = fac.info();
    for(const auto& cls : fac.classInfos())
    {
      if(cls.category() == kVstAudioEffectClass)
      {
        auto str = fmt::format(
            R"_({{
"UID":"{}",
"Cardinality":{},
"Category":"{}",
"Name":"{}",
"Vendor":"{}",
"Version":"{}",
"SDKVersion":"{}",
"Subcategories":"{}",
"ClassFlags":{}
}})_",
            cls.ID().toString(), cls.cardinality(), json_escape(cls.category()),
            json_escape(cls.name()), json_escape(cls.vendor()),
            json_escape(cls.version()), json_escape(cls.sdkVersion()),
            json_escape(cls.subCategoriesString()), (uint32_t)cls.classFlags());

        arr.push_back(str);
      }
    }

    root = fmt::format(
        R"_({{
"Name":"{}",
"Url":"{}",
"Email":"{}",
"Path":"{}",
"Request":{},
"Token":"{}",
"Classes":[
)_",
        json_escape(module->getName()), json_escape(fi.url()), json_escape(fi.email()),
        json_escape(path), id, json_escape(token));
    for(std::size_t i = 0; i < arr.size(); i++)
    {
      root += arr[i];
      if(i < arr.size() - 1)
        root += ',';
    }
    root += "]\n}";

    return root;
  }
  catch(const std::runtime_error& e)
  {
    std::cerr << e.what() << std::endl;
    return error_json(path, id, token, e.what());
  }
  catch(...)
  {
    return error_json(path, id, token, "Unknown exception");
  }
}

void init_invisible_window();
int main(int argc, char** argv)
{
  init_invisible_window();

  return score::puppet::puppet_main(
      argc, argv, 37588, "vst3puppet", true, 30,
      [](const std::string& path, int id, const std::string& token) {
    return load_vst(path, id, token);
      });
}

#include <score/tools/WinMainToMain.hpp>
