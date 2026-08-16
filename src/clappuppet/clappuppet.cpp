#include <score/tools/PuppetClient.hpp>

#include <clap/all.h>

#include <filesystem>
#include <iostream>

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

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

std::string load_clap(const std::string& path, int id, const std::string& token)
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

    // Load the plugin library
#if defined(_WIN32)
    HMODULE handle = LoadLibraryA(path.c_str());
    if(!handle)
    {
      std::cerr << "Failed to load library: " << path << std::endl;
      return error_json("Failed to load library");
    }

    auto entry_fn = (const clap_plugin_entry_t*)GetProcAddress(handle, "clap_entry");
#else
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if(!handle)
    {
      const char* err = dlerror();
      std::cerr << "Failed to load library: " << path << " - " << (err ? err : "")
                << std::endl;
      return error_json(std::string{"Failed to load library: "} + (err ? err : ""));
    }
    
    auto entry_fn = (const clap_plugin_entry_t*)dlsym(handle, "clap_entry");
#endif

    if(!entry_fn)
    {
      std::cerr << "No clap_entry found in: " << path << std::endl;
#if defined(_WIN32)
      FreeLibrary(handle);
#else
      dlclose(handle);
#endif
      return error_json("No clap_entry found");
    }

    if(!entry_fn->init(path.c_str()))
    {
      std::cerr << "Failed to initialize CLAP plugin: " << path << std::endl;
#if defined(_WIN32)
      FreeLibrary(handle);
#else
      dlclose(handle);
#endif
      return error_json("Failed to initialize CLAP plugin");
    }

    auto factory = (const clap_plugin_factory_t*)entry_fn->get_factory(CLAP_PLUGIN_FACTORY_ID);
    if(!factory)
    {
      std::cerr << "No plugin factory found in: " << path << std::endl;
      entry_fn->deinit();
#if defined(_WIN32)
      FreeLibrary(handle);
#else
      dlclose(handle);
#endif
      return error_json("No plugin factory found");
    }

    std::string root = fmt::format(
        R"_({{
"Path":"{}",
"Request":{},
"Token":"{}",
"Plugins":[
)_",
        json_escape(path), id, json_escape(token));

    uint32_t plugin_count = factory->get_plugin_count(factory);
    bool first_plugin = true;
    for(uint32_t i = 0; i < plugin_count; ++i)
    {
      const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, i);
      if(!desc)
        continue;

      std::string features_str = "[";
      if(desc->features)
      {
        bool first = true;
        for(const char* const* feature = desc->features; *feature; ++feature)
        {
          if(!first) features_str += ",";
          features_str += fmt::format("\"{}\"", json_escape(*feature));
          first = false;
        }
      }
      features_str += "]";

      auto plugin_json = fmt::format(
          R"_({{
"ID":"{}",
"Name":"{}",
"Vendor":"{}",
"Version":"{}",
"URL":"{}",
"ManualURL":"{}",
"SupportURL":"{}",
"Description":"{}",
"Features":{}
}})_",
          json_escape(desc->id ? desc->id : ""),
          json_escape(desc->name ? desc->name : ""),
          json_escape(desc->vendor ? desc->vendor : ""),
          json_escape(desc->version ? desc->version : ""),
          json_escape(desc->url ? desc->url : ""),
          json_escape(desc->manual_url ? desc->manual_url : ""),
          json_escape(desc->support_url ? desc->support_url : ""),
          json_escape(desc->description ? desc->description : ""),
          features_str);

      if(!first_plugin)
        root += ",\n";
      root += plugin_json;
      first_plugin = false;
    }

    root += "]\n}";

    entry_fn->deinit();
#if defined(_WIN32)
    FreeLibrary(handle);
#else
    dlclose(handle);
#endif

    return root;
  }
  catch(const std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
    return error_json(std::string{"Exception: "} + e.what());
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
      argc, argv, 37589, "clappuppet", true, 30,
      [](const std::string& path, int id, const std::string& token) {
    return load_clap(path, id, token);
      });
}

#include <score/tools/WinMainToMain.hpp>
