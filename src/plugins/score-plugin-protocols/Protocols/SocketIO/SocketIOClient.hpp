#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/json.hpp>
#include <ossia/detail/fmt.hpp>
#include <charconv>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>

namespace ossia
{
namespace sio
{

class socketio_client_session
    : public std::enable_shared_from_this<socketio_client_session>
{
  using promise_type = boost::asio::detail::awaitable_handler<
      boost::asio::any_io_executor, std::string>;
  using shared_promise_type = std::shared_ptr<promise_type>;

  using websocket_type = boost::beast::websocket::stream<boost::beast::tcp_stream>;

  template <typename T>
  using awaitable = boost::asio::awaitable<T>;

  boost::asio::io_context& m_context;
  boost::beast::flat_buffer m_read_buffer;
  std::string m_host;

  std::unique_ptr<websocket_type> m_stream;
  std::vector<std::pair<int64_t, shared_promise_type>> m_requests;

  boost::asio::experimental::channel<void(boost::system::error_code, const std::string&)>
      m_write_channel;

  struct
  {
    std::string sid;
    std::string socketio_sid;
    std::chrono::milliseconds ping_interval{25000};
    std::chrono::milliseconds ping_timeout{20000};
    int64_t max_payload{5000000};
    bool can_websocket{false};
  } m_socketio_config;

  int64_t m_current_acknowledgement{};

  enum engine_io_packet_type
  {
    OPEN = '0',
    CLOSE = '1',
    PING = '2',
    PONG = '3',
    MESSAGE = '4',
    UPGRADE = '5',
    NOOP = '6',
  };

  enum socket_io_event_type
  {
    CONNECT = '0',
    DISCONNECT = '1',
    EVENT = '2',
    ACK = '3',
    CONNECT_ERROR = '4',
    BINARY_EVENT = '5',
    BINARY_ACK = '6',
  };

  static std::optional<int> consume_int(std::string_view& input)
  {
    int out;
    const std::from_chars_result result
        = std::from_chars(input.data(), input.data() + input.size(), out);
    if(result.ec == std::errc::invalid_argument
       || result.ec == std::errc::result_out_of_range)
    {
      return std::nullopt;
    }
    int n = result.ptr - input.data();
    input = input.substr(n);
    return out;
  }

  static void fail(boost::beast::error_code ec, char const* what)
  {
    std::cerr << what << ": " << ec.message() << "\n";
  }

  void reset_stream_expiry()
  {
    auto& stream = boost::beast::get_lowest_layer(*m_stream);
    stream.expires_after(
        this->m_socketio_config.ping_interval + this->m_socketio_config.ping_timeout);
  }

public:
  explicit socketio_client_session(boost::asio::io_context& ioc)
      : m_context{ioc}
      , m_write_channel(ioc)
  {
  }

  std::function<awaitable<void>()> onConnected;
  std::function<awaitable<void>(boost::json::value)> onEvent;

  awaitable<std::string> http(
      boost::beast::http::verb v, boost::beast::tcp_stream& stream,
      std::string_view target, std::string_view body = "")
  {
    namespace http = boost::beast::http;
    boost::beast::http::request<http::string_body> req{v, target, 11};
    req.set(http::field::host, m_host);
    req.set(http::field::user_agent, "ossia score");
    req.set(http::field::connection, "keep-alive");
    if(!body.empty())
    {
      req.set(http::field::content_length, std::to_string(body.size()));
      req.body() = body;
    }

    co_await http::async_write(stream, req);
    m_read_buffer.clear();
    http::response<http::string_body> res;
    co_await http::async_read(stream, m_read_buffer, res);
    co_return res.body();
  }

  awaitable<std::string> get(boost::beast::tcp_stream& stream, std::string_view target)
  {
    namespace http = boost::beast::http;
    return this->http(http::verb::get, stream, target);
  }

  awaitable<std::string>
  post(boost::beast::tcp_stream& stream, std::string_view target, std::string_view body)
  {
    namespace http = boost::beast::http;
    return this->http(http::verb::post, stream, target, body);
  }

  bool parse_socketio_init(std::string_view str)
  {
    if(str.size() < 4)
      return false;
    if(!str.starts_with("40{"))
      return false;
    if(int end = str.find('\x1e'))
      str = str.substr(0, end);

    str = str.substr(2);
    auto json = boost::json::parse(str);
    if(auto obj = json.try_as_object())
    {
      auto& o = *obj;
      if(auto k = o.find("sid"))
      {
        auto val = k->value();
        if(auto sid = val.try_as_string())
        {
          m_socketio_config.socketio_sid = *sid;
          return true;
        }
      }
    }
    return false;
  }

  bool parse_engineio_init(std::string_view str)
  {
    if(str.size() < 12)
      return false;
    if(!str.starts_with("0{"))
      return false;

    str = str.substr(1);
    auto json = boost::json::parse(str);
    if(auto obj = json.try_as_object())
    {
      auto& o = *obj;
      if(auto k = o.find("sid"))
      {
        auto val = k->value();
        if(auto sid = val.try_as_string())
          m_socketio_config.sid = *sid;
      }
      if(auto k = o.find("upgrades"))
      {
        auto val = k->value();
        if(auto v = val.try_as_array())
          if(v->size() > 0 && v->front() == "websocket")
            m_socketio_config.can_websocket = true;
      }
      if(auto k = o.find("pingInterval"))
      {
        auto val = k->value();
        if(auto v = val.try_as_int64())
          m_socketio_config.ping_interval = std::chrono::milliseconds(*v);
      }
      if(auto k = o.find("pingTimeout"))
      {
        auto val = k->value();
        if(auto v = val.try_as_int64())
          m_socketio_config.ping_timeout = std::chrono::milliseconds(*v);
      }
      if(auto k = o.find("maxPayload"))
      {
        auto val = k->value();
        if(auto v = val.try_as_int64())
          m_socketio_config.max_payload = *v;
      }
    }
    return !this->m_socketio_config.sid.empty();
  }

  awaitable<void> start_session(std::string host, std::string port)
  {
    static constexpr auto init_timeout = std::chrono::seconds(3);
    auto executor = co_await boost::asio::this_coro::executor;
    auto resolver = boost::asio::ip::tcp::resolver{executor};

    m_read_buffer.reserve(5000000);
    auto const results = co_await resolver.async_resolve(host, port);
    namespace websocket = boost::beast::websocket;

    m_stream = std::make_unique<websocket::stream<boost::beast::tcp_stream>>(executor);
    m_stream->set_option(
        websocket::stream_base::timeout::suggested(boost::beast::role_type::client));
    m_stream->set_option(
        websocket::stream_base::decorator([](websocket::request_type& req) {
      req.set(boost::beast::http::field::user_agent, "ossia score");
    }));

    /// First step is a normal http connection
    auto& stream = boost::beast::get_lowest_layer(*m_stream);

    stream.expires_after(init_timeout);
    co_await stream.async_connect(results);
    // 1. Engine.io connection
    {
      stream.expires_after(init_timeout);
      auto rep = co_await get(stream, "/socket.io/?EIO=4&transport=polling");
      if(!parse_engineio_init(rep))
        throw boost::system::system_error(
            boost::asio::error::invalid_argument, "cannot parse eio init");
    }

    // 2. Socket.io connection request
    {
      stream.expires_after(init_timeout);
      auto rep = co_await post(
          stream,
          "/socket.io/?EIO=4&transport=polling&sid=" + this->m_socketio_config.sid,
          "40");
      if(rep != "ok")
        throw boost::system::system_error(
            boost::asio::error::invalid_argument, "sio init request refused");
    }

    // 3. Socket.io connection approval
    {
      stream.expires_after(init_timeout);
      auto rep = co_await get(
          stream,
          "/socket.io/?EIO=4&transport=polling&sid=" + this->m_socketio_config.sid);
      if(!parse_socketio_init(rep))
        throw boost::system::system_error(
            boost::asio::error::invalid_argument, "sio init approval refused");
    }

    // 4. Socket.io connection upgrade
    {
      stream.expires_after(init_timeout);
      co_await m_stream->async_handshake(
          m_host,
          "/socket.io/?EIO=4&transport=websocket&sid=" + this->m_socketio_config.sid);

      stream.expires_after(init_timeout);
      co_await m_stream->async_write(boost::asio::buffer("2probe", 6));

      stream.expires_after(init_timeout);
      m_read_buffer.clear();
      co_await m_stream->async_read(m_read_buffer);
      if(boost::beast::buffers_to_string(m_read_buffer.data()) != "3probe")
        throw std::runtime_error("Probe failed");

      // Upgrade packet
      m_read_buffer.clear();
      stream.expires_after(init_timeout);
      co_await m_stream->async_write(boost::asio::buffer("5", 1));
    }

    /// Now we are fully connected to socket.io over websockets ///
    // Start write loop
    reset_stream_expiry();
    async(write_loop());

    async(onConnected());

    co_await read_loop();

    co_await close_session();
    co_return;
  }

  awaitable<void> write_loop()
  {
    for(;;)
    {
      auto str = co_await m_write_channel.async_receive(boost::asio::use_awaitable);
      // std::cerr << "Sending... " << str << std::endl;
      co_await m_stream->async_write(boost::asio::buffer(str.data(), str.size()));
    }
  }

  awaitable<void> read_loop()
  {
    for(;;)
    {
      m_read_buffer.clear();
      int64_t read = co_await m_stream->async_read(m_read_buffer);
      if(read > 0)
      {
        if(!process_socketio(boost::beast::buffers_to_string(m_read_buffer.data())))
        {
          std::cerr << "Error while reading..." << read << "\n";
        }
      }
    }
  }

  awaitable<void> write_pong()
  {
    reset_stream_expiry();

    co_await m_write_channel.async_send(
        boost::system::error_code{}, std::string(1, engine_io_packet_type::PONG),
        boost::asio::use_awaitable);
  }

  bool process_event(std::string_view data)
  {
    if(auto json = boost::json::parse(data); !json.is_null())
      async(onEvent(std::move(json)));
    return true;
  }

  bool process_ack(std::string_view data)
  {
    if(data[0] >= '0' && data[0] <= '9')
    {
      if(auto req_num = consume_int(data))
      {
        for(auto it = m_requests.begin(); it != this->m_requests.end();)
        {
          if(it->first == req_num)
          {
            (*it->second)(data);

            it = m_requests.erase(it);
            return true;
          }
          else
          {
            ++it;
          }
        }
        return true;
      }
      else
      {
        return false;
      }
    }
    else
    {
      return false;
    }
  }

  void async(auto&& f)
  {
    boost::asio::co_spawn(this->m_context, std::move(f), [](std::exception_ptr e) {
      if(e)
        std::rethrow_exception(e);
    });
  }

  bool process_socketio(std::string_view data)
  {
    if(data.empty())
      return false;
    switch(data[0])
    {
      case OPEN:
        return false;
      case CLOSE:
        async(close_session());
        return true;
      case UPGRADE:
        return false;
      case NOOP:
        return true;
      case engine_io_packet_type::PING:
        async(write_pong());
        return true;
      case engine_io_packet_type::PONG:
        return false;
      case engine_io_packet_type::MESSAGE: {
        data = data.substr(1);
        if(data.empty())
          return false;
        switch(data[0])
        {
          case socket_io_event_type::CONNECT:
            std::cerr << "CONNECT!\n";
            break;
          case socket_io_event_type::DISCONNECT:
            std::cerr << "DISCONNECT!\n";
            break;
          case socket_io_event_type::EVENT:
            data = data.substr(1);
            if(data.empty())
              return false;

            return process_event(data);
            break;
          case socket_io_event_type::ACK:
            data = data.substr(1);
            if(data.empty())
              return false;

            return process_ack(data);
            break;
          case socket_io_event_type::CONNECT_ERROR:
            std::cerr << "CONNECT_ERROR!\n";
            break;
          case socket_io_event_type::BINARY_EVENT:
            std::cerr << "BINARY_EVENT!\n";
            break;
          case socket_io_event_type::BINARY_ACK:
            std::cerr << "BINARY_ACK!\n";
            break;
        }

        return true;
        break;
      }
      default:
        return true;
        break;
    }
    return false;
  }

  /* for server
  awaitable<void> ping()
  {
    using namespace boost::asio::experimental::awaitable_operators;
    auto executor = co_await net::this_coro::executor;
    boost::asio::steady_timer tim{executor, ping_timeout_};
    beast::flat_buffer pingbuf;
    for(;;)
    {
      // Send ping
      co_await write_channel.async_send(
          boost::system::error_code{}, std::string(engine_io_packet_type::PONG, 1), net::use_awaitable);
      last_sent_ping_time_ = std::chrono::steady_clock::now();

      // Wait
      tim.expires_after(ping_interval_);
      co_await tim.async_wait();
    }
  }
*/

  awaitable<void> close_session()
  {
    if(m_stream)
    {
      auto& stream = boost::beast::get_lowest_layer(*m_stream);
      boost::beast::error_code ec;
      stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

      if(ec && ec != boost::beast::errc::not_connected)
        throw boost::system::system_error(ec, "shutdown");
    }
    co_return;
  }

  template <typename ResponseHandler = boost::asio::use_awaitable_t<>>
  auto post(int req, ResponseHandler&& handler = {})
  {
    auto initiate = [this, req]<typename Handler>(Handler&& self) mutable {
      this->m_requests.emplace_back(
          req, std::make_shared<Handler>(std::forward<Handler>(self)));
    };
    return boost::asio::async_initiate<ResponseHandler, void(std::string)>(
        std::move(initiate), std::move(handler));
  }

  awaitable<std::string> request(auto&&... args)
  {
    auto executor = co_await boost::asio::this_coro::executor;
    auto cur = m_current_acknowledgement++;

    awaitable<std::string> q = post(cur, boost::asio::use_awaitable);

    co_await this->m_write_channel.async_send(
        {},
        fmt::format("42{}{}", cur, boost::json::serialize(boost::json::array{args...})));

    co_return co_await std::move(q);
  }

  boost::json::value parse_event_reply(auto&& res)
  {
    auto r = boost::json::parse(res);
    if(auto r_arr = r.try_as_array())
    {
      boost::json::array& r = *r_arr;
      if(r.size() == 2)
        return r.at(1);
    }
    throw std::runtime_error("Invalid reply");
  }

  awaitable<boost::json::value> request_event(std::string event, auto&&... args)
  {
    if constexpr(sizeof...(args) == 0)
    {
      co_return parse_event_reply(co_await request(event, boost::json::array{}));
    }
    else
    {
      co_return parse_event_reply(co_await request(event, args...));
    }
  }

  void run(char const* host, char const* port)
  {
    m_host = host;

    async(start_session(host, port));
  }
};
}
}
