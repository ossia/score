
#include <ossia/detail/fmt.hpp>
#include <ossia/network/sockets/websocket.hpp>

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

// clang-format off
#include <score/tools/InvisibleWindow.hpp>
#undef OK
#undef NO
#undef Status
// clang-format on

using namespace Steinberg;
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

    std::string err;
    auto module = VST3::Hosting::Module::create(path, err);

    if(!module)
    {
      std::cerr << "Failed to load VST3 " << path << err << std::endl;
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
            cls.ID().toString(), cls.cardinality(), cls.category(), cls.name(),
            cls.vendor(), cls.version(), cls.sdkVersion(), cls.subCategoriesString(),
            (double)cls.classFlags());

        arr.push_back(str);
      }
    }

    root = fmt::format(
        R"_({{
"Name":"{}",
"Url":"{}",
"Email":"{}",
"Path":"{}",
"Request":"{}",
"Classes":[
)_",
        module->getName(), fi.url(), fi.email(), path, id);
    for(int i = 0; i < arr.size(); i++)
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
  }
  return {};
}

struct app
{

  boost::asio::io_context ctx;
  ossia::net::websocket_simple_client socket{{.url = "ws://127.0.0.1:37588"}, ctx};

  bool socket_ready{}, vst_ready{};
  std::string json_ret;

  app()
  {
    socket.on_open.connect<&app::on_open>(*this);
    socket.on_fail.connect<&app::on_error>(*this);
    socket.on_close.connect<&app::on_error>(*this);

    socket.websocket_client::connect("ws://127.0.0.1:37588");
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

int main(int argc, char** argv)
{
  if(argc <= 1)
    return 1;

  int id = 0;
  if(argc > 2)
    id = std::atoi(argv[2]);

  app a;
  invisible_window w;

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
