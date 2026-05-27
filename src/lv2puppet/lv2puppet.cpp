#include <ossia/detail/fmt.hpp>
#include <ossia/network/sockets/websocket.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

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

#include <cstring>

#include <lilv/lilv.h>
#include <lv2/lv2plug.in/ns/lv2core/lv2.h>
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/midi/midi.h>
#include <lv2/lv2plug.in/ns/ext/presets/presets.h>

namespace
{
std::string json_escape(const char* s)
{
  if(!s)
    return {};
  std::string out;
  out.reserve(std::strlen(s) + 8);
  for(const char* p = s; *p; ++p)
  {
    unsigned char c = static_cast<unsigned char>(*p);
    switch(c)
    {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if(c < 0x20)
          out += fmt::format("\\u{:04x}", (int)c);
        else
          out += (char)c;
        break;
    }
  }
  return out;
}

std::string node_string(const LilvNode* n)
{
  if(!n)
    return {};
  if(const char* s = lilv_node_as_string(n))
    return json_escape(s);
  return {};
}

std::string node_uri(const LilvNode* n)
{
  if(!n)
    return {};
  if(lilv_node_is_uri(n))
  {
    if(const char* s = lilv_node_as_uri(n))
      return json_escape(s);
  }
  if(const char* s = lilv_node_as_string(n))
    return json_escape(s);
  return {};
}

struct PortCounts
{
  int audio_in = 0;
  int audio_out = 0;
  int midi_in = 0;
  int midi_out = 0;
  int atom_in = 0;
  int atom_out = 0;
  int control_in = 0;
  int control_out = 0;
  int cv_in = 0;
};

PortCounts count_ports(LilvWorld* world, const LilvPlugin* p)
{
  PortCounts c;
  LilvNode* lv2_InputPort = lilv_new_uri(world, LILV_URI_INPUT_PORT);
  LilvNode* lv2_OutputPort = lilv_new_uri(world, LILV_URI_OUTPUT_PORT);
  LilvNode* lv2_AudioPort = lilv_new_uri(world, LILV_URI_AUDIO_PORT);
  LilvNode* lv2_ControlPort = lilv_new_uri(world, LILV_URI_CONTROL_PORT);
  LilvNode* lv2_AtomPort = lilv_new_uri(world, LV2_ATOM__AtomPort);
  LilvNode* lv2_CVPort = lilv_new_uri(world, LV2_CORE__CVPort);
  LilvNode* lv2_MidiEvent = lilv_new_uri(world, LILV_URI_MIDI_EVENT);
  // atom:supports avoids the deprecated ev:supportsEvent path
  LilvNode* atom_supports = lilv_new_uri(world, LV2_ATOM__supports);

  const uint32_t n = lilv_plugin_get_num_ports(p);
  for(uint32_t i = 0; i < n; ++i)
  {
    const LilvPort* port = lilv_plugin_get_port_by_index(p, i);
    if(!port)
      continue;
    const bool in = lilv_port_is_a(p, port, lv2_InputPort);
    const bool out = lilv_port_is_a(p, port, lv2_OutputPort);

    if(lilv_port_is_a(p, port, lv2_AudioPort))
    {
      if(in)
        ++c.audio_in;
      else if(out)
        ++c.audio_out;
    }
    else if(lilv_port_is_a(p, port, lv2_AtomPort))
    {
      LilvNodes* supports = lilv_port_get_value(p, port, atom_supports);
      const bool is_midi
          = supports && lilv_nodes_contains(supports, lv2_MidiEvent);
      lilv_nodes_free(supports);
      if(is_midi)
      {
        if(in)
          ++c.midi_in;
        else if(out)
          ++c.midi_out;
      }
      else
      {
        if(in)
          ++c.atom_in;
        else if(out)
          ++c.atom_out;
      }
    }
    else if(lilv_port_is_a(p, port, lv2_CVPort))
    {
      if(in)
        ++c.cv_in;
    }
    else if(lilv_port_is_a(p, port, lv2_ControlPort))
    {
      if(in)
        ++c.control_in;
      else if(out)
        ++c.control_out;
    }
  }

  lilv_node_free(atom_supports);
  lilv_node_free(lv2_InputPort);
  lilv_node_free(lv2_OutputPort);
  lilv_node_free(lv2_AudioPort);
  lilv_node_free(lv2_ControlPort);
  lilv_node_free(lv2_AtomPort);
  lilv_node_free(lv2_CVPort);
  lilv_node_free(lv2_MidiEvent);
  return c;
}

std::string make_bundle_uri(std::string path)
{
  for(char& c : path)
    if(c == '\\')
      c = '/';
  while(!path.empty() && path.back() == '/')
    path.pop_back();
  // Windows drive paths: file:// + C:/foo -> file:///C:/foo
  if(!path.empty() && path[0] != '/')
    return std::string{"file:///"} + path + "/";
  return std::string{"file://"} + path + "/";
}

std::string scan_bundle(const std::string& bundle_path, int id)
{
  try
  {
    if(!std::filesystem::exists(bundle_path))
    {
      std::cerr << "Invalid bundle path: " << bundle_path << std::endl;
      return fmt::format(
          R"_({{"Bundle":"{}","Request":{},"Error":"Bundle not found","Plugins":[]}})_",
          json_escape(bundle_path.c_str()), id);
    }

    LilvWorld* world = lilv_world_new();
    if(!world)
    {
      return fmt::format(
          R"_({{"Bundle":"{}","Request":{},"Error":"lilv_world_new failed","Plugins":[]}})_",
          json_escape(bundle_path.c_str()), id);
    }

    const std::string bundle_uri_str = make_bundle_uri(bundle_path);
    LilvNode* bundle_uri = lilv_new_uri(world, bundle_uri_str.c_str());
    if(!bundle_uri)
    {
      lilv_world_free(world);
      return fmt::format(
          R"_({{"Bundle":"{}","Request":{},"Error":"Invalid bundle URI","Plugins":[]}})_",
          json_escape(bundle_path.c_str()), id);
    }

    // Avoid lilv_world_load_all: one malformed manifest would crash this scan
    if(const char* specs_env = std::getenv("LV2PUPPET_SPECS"))
    {
#if defined(_WIN32)
      const char sep = ';';
#else
      const char sep = ':';
#endif
      std::string_view specs{specs_env};
      while(!specs.empty())
      {
        auto pos = specs.find(sep);
        std::string spec_path
            = std::string{specs.substr(0, pos == std::string_view::npos ? specs.size() : pos)};
        specs.remove_prefix(pos == std::string_view::npos ? specs.size() : pos + 1);
        if(spec_path.empty())
          continue;
        const std::string spec_uri = make_bundle_uri(spec_path);
        if(LilvNode* spec_node = lilv_new_uri(world, spec_uri.c_str()))
        {
          lilv_world_load_bundle(world, spec_node);
          lilv_node_free(spec_node);
        }
      }
    }
    lilv_world_load_bundle(world, bundle_uri);

    lilv_world_load_specifications(world);
    lilv_world_load_plugin_classes(world);

    const LilvPlugins* plugins = lilv_world_get_all_plugins(world);

    std::string plugins_json;
    bool first = true;

    LilvNode* pset_node = lilv_new_uri(world, LV2_PRESETS__Preset);
    LilvNode* rdfs_seeAlso
        = lilv_new_uri(world, "http://www.w3.org/2000/01/rdf-schema#seeAlso");

    LILV_FOREACH(plugins, it, plugins)
    {
      const LilvPlugin* p = lilv_plugins_get(plugins, it);
      if(!p)
        continue;

      // Emit only plug-ins from the requested bundle
      const LilvNode* p_bundle = lilv_plugin_get_bundle_uri(p);
      const char* p_bundle_str = p_bundle ? lilv_node_as_string(p_bundle) : nullptr;
      if(!p_bundle_str || bundle_uri_str != p_bundle_str)
        continue;

      const LilvNode* uri_node = lilv_plugin_get_uri(p);
      const std::string uri = node_uri(uri_node);

      LilvNode* name_node = lilv_plugin_get_name(p);
      const std::string name = node_string(name_node);
      if(name_node)
        lilv_node_free(name_node);

      std::string class_label;
      if(const LilvPluginClass* cls = lilv_plugin_get_class(p))
      {
        const LilvNode* lab = lilv_plugin_class_get_label(cls);
        if(lab)
          class_label = node_string(lab);
      }

      LilvNode* author_node = lilv_plugin_get_author_name(p);
      const std::string author = node_string(author_node);
      if(author_node)
        lilv_node_free(author_node);

      LilvNode* homepage_node = lilv_plugin_get_author_homepage(p);
      const std::string doc_link = node_string(homepage_node);
      if(homepage_node)
        lilv_node_free(homepage_node);

      const PortCounts pc = count_ports(world, p);

      bool has_ui = false;
      std::string ui_uris_json = "[";
      bool first_ui = true;
      if(LilvUIs* uis = lilv_plugin_get_uis(p))
      {
        LILV_FOREACH(uis, ui_it, uis)
        {
          const LilvUI* ui = lilv_uis_get(uis, ui_it);
          if(!ui)
            continue;
          has_ui = true;
          const LilvNodes* types = lilv_ui_get_classes(ui);
          if(types)
          {
            LILV_FOREACH(nodes, t_it, types)
            {
              const LilvNode* t = lilv_nodes_get(types, t_it);
              if(!t)
                continue;
              if(!first_ui)
                ui_uris_json += ",";
              ui_uris_json += "\"" + node_uri(t) + "\"";
              first_ui = false;
            }
          }
        }
        lilv_uis_free(uis);
      }
      ui_uris_json += "]";

      // rdfs:seeAlso points to the bundle TTL; parent dir is the preset bundle
      std::string preset_uris_json = "[";
      std::string preset_bundles_json = "[";
      bool first_pb = true;
      if(LilvNodes* presets = lilv_plugin_get_related(p, pset_node))
      {
        LILV_FOREACH(nodes, pit, presets)
        {
          const LilvNode* preset = lilv_nodes_get(presets, pit);
          if(!preset)
            continue;

          std::string bundle_uri;
          if(LilvNodes* seeAlso
             = lilv_world_find_nodes(world, preset, rdfs_seeAlso, nullptr))
          {
            if(const LilvNode* sa = lilv_nodes_get_first(seeAlso))
            {
              if(const char* sa_str = lilv_node_as_uri(sa))
              {
                std::string s{sa_str};
                auto slash = s.find_last_of('/');
                if(slash != std::string::npos)
                  bundle_uri = s.substr(0, slash + 1);
              }
            }
            lilv_nodes_free(seeAlso);
          }
          if(bundle_uri.empty())
          {
            // Fall back to preset URI's parent; raw URI (json_escape runs below)
            const char* raw = nullptr;
            if(lilv_node_is_uri(preset))
              raw = lilv_node_as_uri(preset);
            else if(lilv_node_as_string(preset))
              raw = lilv_node_as_string(preset);
            if(raw)
            {
              std::string s{raw};
              auto slash = s.find_last_of('/');
              if(slash != std::string::npos)
                bundle_uri = s.substr(0, slash + 1);
            }
          }

          if(!first_pb)
          {
            preset_uris_json += ",";
            preset_bundles_json += ",";
          }
          preset_uris_json += "\"" + node_uri(preset) + "\"";
          preset_bundles_json += "\"" + json_escape(bundle_uri.c_str()) + "\"";
          first_pb = false;
        }
        lilv_nodes_free(presets);
      }
      preset_uris_json += "]";
      preset_bundles_json += "]";

      auto plugin_json = fmt::format(
          R"_({{"uri":"{}","name":"{}","class":"{}","author":"{}","documentationLink":"{}","audio_in":{},"audio_out":{},"midi_in":{},"midi_out":{},"control_in":{},"control_out":{},"atom_in":{},"atom_out":{},"cv_in":{},"has_ui":{},"ui_uris":{},"preset_uris":{},"preset_bundles":{}}})_",
          uri, name, class_label, author, doc_link,
          pc.audio_in, pc.audio_out, pc.midi_in, pc.midi_out,
          pc.control_in, pc.control_out, pc.atom_in, pc.atom_out, pc.cv_in,
          has_ui ? "true" : "false", ui_uris_json, preset_uris_json,
          preset_bundles_json);

      if(!first)
        plugins_json += ",\n";
      plugins_json += plugin_json;
      first = false;
    }

    lilv_node_free(rdfs_seeAlso);
    lilv_node_free(pset_node);
    lilv_node_free(bundle_uri);
    lilv_world_free(world);

    return fmt::format(
        R"_({{"Bundle":"{}","Request":{},"Plugins":[{}]}})_",
        json_escape(bundle_path.c_str()), id, plugins_json);
  }
  catch(const std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
    return fmt::format(
        R"_({{"Bundle":"{}","Request":{},"Error":"Exception: {}","Plugins":[]}})_",
        json_escape(bundle_path.c_str()), id, json_escape(e.what()));
  }
  catch(...)
  {
    std::cerr << "Unknown exception" << std::endl;
    return fmt::format(
        R"_({{"Bundle":"{}","Request":{},"Error":"Unknown exception","Plugins":[]}})_",
        json_escape(bundle_path.c_str()), id);
  }
}

struct app
{
  boost::asio::io_context ctx;
  ossia::net::websocket_simple_client socket{
      {.url = "ws://127.0.0.1:37590"}, ctx};

  bool socket_ready{}, lv2_ready{};
  std::string json_ret;

  app()
  {
    socket.on_open.connect<&app::on_open>(*this);
    socket.on_fail.connect<&app::on_error>(*this);
    socket.on_close.connect<&app::on_error>(*this);

    socket.websocket_client::connect("ws://127.0.0.1:37590");
  }

  std::string bundle_for_log;

  // _Exit: websocketpp/asio destructors throw from internals past our handlers
  void finish_success() { std::_Exit(json_ret.empty() ? 1 : 0); }

  void on_ready()
  {
    if(socket_ready && lv2_ready)
    {
      try
      {
        socket.send_message(json_ret);
      }
      catch(...)
      {
        on_error();
        return;
      }

      // Flush delay: send_message is async; large payloads (~MB) need time
      auto delay = std::make_shared<boost::asio::steady_timer>(ctx);
      delay->expires_after(std::chrono::milliseconds(500));
      delay->async_wait([this, delay](auto) { finish_success(); });
    }
  }

  void on_error()
  {
    auto line = fmt::format("[lv2puppet {}] socket error\n", bundle_for_log);
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);
    std::_Exit(1);
  }

  void load(const std::string& bundle, int id)
  {
    auto pos = bundle.find_last_of('/');
    bundle_for_log = (pos == std::string::npos) ? bundle : bundle.substr(pos + 1);
    json_ret = scan_bundle(bundle, id);
    // No stdout echo: parent pipe is never drained; large JSON would block
    lv2_ready = true;
    on_ready();
  }

  void on_open()
  {
    socket_ready = true;
    on_ready();
  }
};
}

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
