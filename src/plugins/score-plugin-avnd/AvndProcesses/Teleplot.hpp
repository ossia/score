#pragma once
#include <ossia/network/resolve.hpp>
#include <ossia/network/sockets/udp_socket.hpp>

#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/udp.hpp>

#include <QDebug>

#include <AvndProcesses/AddressTools.hpp>
namespace avnd_tools
{
/** Sends the input over Teleplot
 */
struct Teleplot : PatternObject
{
  halp_meta(name, "Teleplot")
  halp_meta(author, "ossia team")
  halp_meta(category, "Monitoring")
  halp_meta(description, "Forwards a set of device explorer messages to ")
  halp_meta(c_name, "avnd_teleplot")
  halp_meta(manual_url, "https://ossia.io/score-docs/processes/teleplot.html")
  halp_meta(uuid, "e1d5b9a0-4df9-4281-87a6-9f427dfb6e31")

  // Populated automatically from Executor
  boost::asio::io_context* io_context{};

  struct inputs_t
  {
    PatternSelector pattern;
    struct : halp::lineedit<"Host", "">
    {
      void update(Teleplot& self) { self.update(); }
    } host;
  } inputs;

  struct
  {
  } outputs;

  ~Teleplot() { clear(); }

  std::pair<std::string, uint16_t> resolve_ip(const std::string& url)
  {
    auto [host, port] = ossia::url_to_host_and_port(url);
    if(auto resolved = ossia::resolve_sync_v4<boost::asio::ip::udp>(host, port))
    {
      return {resolved->host, std::stoi(resolved->port)};
    }
    return {};
  }

  void clear()
  {
    for(auto& [param, cb] : params)
    {
      param->remove_callback(cb);
    }
    params.clear();
  }

  void update()
  {
    // 1. Remove existing callbacks
    clear();

    // 2. recreate socket
    {
      //      auto split = QString::fromStdString(inputs.host.value).split(':');
      auto [ip, port] = resolve_ip(inputs.host.value);
      if(!ip.empty() && port > 1)
      {
        socket = std::make_shared<ossia::net::udp_send_socket>(
            ossia::net::outbound_socket_configuration{.host = ip, .port = port},
            *io_context);
        socket->connect();
      }
    }

    // 3. Recreate callbacks
    for(auto nodes : this->roots)
    {
      if(auto p = nodes->get_parameter(); p && !params.contains(p))
      {
        auto it = p->add_callback(
            [p, socket = socket](const ossia::value& v) { push(*socket, *p, v); });
        params.emplace(p, it);
      }
    }
  }

  // NOTE: this function can be called from any thread
  static void push(
      ossia::net::udp_send_socket& socket, const ossia::net::parameter_base& param,
      const ossia::value& v)
  {
    using clk = std::chrono::system_clock;

    thread_local fmt::memory_buffer buf;
    buf.clear();
    buf.reserve(512);

    struct
    {
      const std::string& addr;
      int64_t t = std::chrono::duration_cast<std::chrono::milliseconds>(
                      clk::now().time_since_epoch())
                      .count();

      void operator()(ossia::impulse v)
      {
        fmt::format_to(fmt::appender(buf), "{}:{}:1\n", addr, t);
      }
      void operator()(int v)
      {
        fmt::format_to(fmt::appender(buf), "{}:{}:{}\n", addr, t, v);
      }
      void operator()(float v)
      {
        fmt::format_to(fmt::appender(buf), "{}:{}:{}\n", addr, t, v);
      }
      void operator()(std::string v)
      {
        fmt::format_to(fmt::appender(buf), "{}:{}:{}\n", addr, t, v);
      }
      void operator()(bool v)
      {
        fmt::format_to(fmt::appender(buf), "{}:{}:{}\n", addr, t, v ? 1 : 0);
      }
      void operator()(ossia::vec2f v)
      {
        fmt::format_to(
            fmt::appender(buf), "{}.x:{}:{}\n{}.y:{}:{}\n", addr, t, v[0], addr, t,
            v[1]);
      }
      void operator()(ossia::vec3f v)
      {
        fmt::format_to(
            fmt::appender(buf), "{}.x:{}:{}\n{}.y:{}:{}\n{}.z:{}:{}\n", addr, t, v[0],
            addr, t, v[1], addr, t, v[2]);
      }
      void operator()(ossia::vec4f v)
      {
        fmt::format_to(
            fmt::appender(buf), "{}.x:{}:{}\n{}.y:{}:{}\n{}.z:{}:{}\n{}.w:{}:{}\n", addr,
            t, v[0], addr, t, v[1], addr, t, v[2], addr, t, v[3]);
      }
      void operator()(const std::vector<ossia::value>& v)
      {
        int i = 0;
        for(auto& val : v)
        {
          // FIXME this does not handle multidimensional arrays / recursivity.
          fmt::format_to(
              fmt::appender(buf), "{}[{}]:{}:{}\n", addr, i, t,
              ossia::convert<double>(val));
          i++;
        }
      }
      void operator()(const ossia::value_map_type& v)
      {
        for(auto& [k, val] : v)
        {
          // FIXME this does not handle multidimensional arrays / recursivity.
          fmt::format_to(
              fmt::appender(buf), "{}[{}]:{}:{}\n", addr, k, t,
              ossia::convert<double>(val));
        }
      }
      void operator()() { }

    } vis{.addr = param.get_node().osc_address()};
    v.apply(vis);

    socket.write(buf.begin(), buf.size());
  }

  void operator()()
  {
    if(!socket)
    {
      socket = std::make_shared<ossia::net::udp_send_socket>(
          ossia::net::outbound_socket_configuration{.host = "127.0.0.1", .port = 47269},
          *io_context);
      socket->connect();
    }

    if(m_paths.empty())
      return;

    // FIXME do this in an update callback instead
    // Create callbacks for added nodes
    for(auto nodes : this->roots)
    {
      if(auto p = nodes->get_parameter(); p && !params.contains(p))
      {
        auto it = p->add_callback(
            [p, socket = socket](const ossia::value& v) { push(*socket, *p, v); });
        params.emplace(p, it);
      }
    }

    // Remove callbacks for removed nodes
    for(auto it = params.begin(); it != params.end();)
    {
      if(!ossia::contains(this->roots, &(it->first)->get_node()))
      {
        it->first->remove_callback(it->second);
        it = params.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

  boost::container::flat_map<
      ossia::net::parameter_base*,
      ossia::callback_container<ossia::value_callback>::iterator>
      params;
  std::shared_ptr<ossia::net::udp_send_socket> socket;
};
}
