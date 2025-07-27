#include <ossia/detail/fmt.hpp>
#include <ossia/network/sockets/websocket.hpp>

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

std::string load_clap(const std::string& path, int id)
{
  try
  {
    const bool isFile = std::filesystem::exists(path);
    if(!isFile)
    {
      std::cerr << "Invalid path: " << path << std::endl;
      return {};
    }

    // Load the plugin library
#if defined(_WIN32)
    HMODULE handle = LoadLibraryA(path.c_str());
    if(!handle)
    {
      std::cerr << "Failed to load library: " << path << std::endl;
      return {};
    }
    
    auto entry_fn = (const clap_plugin_entry_t*)GetProcAddress(handle, "clap_entry");
#else
    void* handle = dlopen(path.c_str(), RTLD_LAZY);
    if(!handle)
    {
      std::cerr << "Failed to load library: " << path << " - " << dlerror() << std::endl;
      return {};
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
      return {};
    }

    if(!entry_fn->init(path.c_str()))
    {
      std::cerr << "Failed to initialize CLAP plugin: " << path << std::endl;
#if defined(_WIN32)
      FreeLibrary(handle);
#else
      dlclose(handle);
#endif
      return {};
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
      return {};
    }

    std::string root = fmt::format(
        R"_({{
"Path":"{}",
"Request":{},
"Plugins":[
)_",
        path, id);

    uint32_t plugin_count = factory->get_plugin_count(factory);
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
          features_str += fmt::format("\"{}\"", *feature);
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
          desc->id ? desc->id : "",
          desc->name ? desc->name : "",
          desc->vendor ? desc->vendor : "",
          desc->version ? desc->version : "",
          desc->url ? desc->url : "",
          desc->manual_url ? desc->manual_url : "",
          desc->support_url ? desc->support_url : "",
          desc->description ? desc->description : "",
          features_str);

      if(i > 0)
        root += ",\n";
      root += plugin_json;
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
  }
  return {};
}

struct app
{

  boost::asio::io_context ctx;
  ossia::net::websocket_simple_client socket{{.url = "ws://127.0.0.1:37589"}, ctx};

  bool socket_ready{}, clap_ready{};
  std::string json_ret;

  app()
  {
    socket.on_open.connect<&app::on_open>(*this);
    socket.on_fail.connect<&app::on_error>(*this);
    socket.on_close.connect<&app::on_error>(*this);

    socket.websocket_client::connect("ws://127.0.0.1:37589");
  }

  void on_ready()
  {
    if(socket_ready && clap_ready)
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

  void load(const std::string& clap, int id)
  {
    json_ret = load_clap(clap, id);
    std::cout << json_ret << "\n";
    clap_ready = true;
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

#include <score/tools/WinMainToMain.hpp>
