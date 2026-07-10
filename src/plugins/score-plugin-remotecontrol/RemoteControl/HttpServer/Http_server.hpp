//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#pragma once

#define BOOST_DATE_TIME_NO_LIB 1

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>

#include <memory>
#include <string>
#include <thread>

#ifdef _WIN32
#define SHUT_RDWR 2
#endif

#include <QDir>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QtDebug>
#include <QApplication>
#include <QFile>

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>

namespace RemoteControl
{
class Http_server
{
public:
  Http_server();
  ~Http_server();

  void start_thread();
  void stop_thread();
  void set_path(const std::string& str);
  void set_address(const std::string& str);
  void set_port(unsigned short prt);

private:
  // Return a reasonable mime type based on the extension of a file.
  beast::string_view mime_type(beast::string_view path);

  // Append an HTTP rel-path to a local filesystem path.
  // The returned path is normalized for the platform.
  std::string path_cat(beast::string_view path);

  // This function produces an HTTP response for the given
  // request. The type of the response object depends on the
  // contents of the request, so the interface requires the
  // caller to pass a generic lambda for receiving the response.
  template<class Body, class Allocator, class Send>
  void handle_request(
      http::request<Body, http::basic_fields<Allocator>>&& req
      , Send&& send);

  // Report a failure
  void fail(beast::error_code ec, char const* what);

  // This is the C++11 equivalent of a generic lambda.
  // The function object is used to send an HTTP message.
  template<class Stream>
  struct send_lambda
  {
    Stream& stream_;
    bool& close_;
    beast::error_code& ec_;

    explicit send_lambda(
        Stream& stream,
        bool& close,
        beast::error_code& ec)
        : stream_(stream)
        , close_(close)
        , ec_(ec)
    { }

    template<bool isRequest, class Body, class Fields>
    void operator()
        (http::message<isRequest, Body, Fields>&& msg) const
    {
      // Determine if we should close the connection after
      close_ = msg.need_eof();

      // We need the serializer here because the serializer requires
      // a non-const file_body, and the message oriented version of
      // http::write only works with const messages.
      http::serializer<isRequest, Body, Fields> sr{msg};
      http::write(stream_, sr, ec_);
    }
  };

  // Handles an HTTP server connection
  void do_session(tcp::socket& socket);

  // Open a server using sockets
  int open_server();
  bool running();

  net::io_context m_ioc;
  tcp::endpoint m_endpoint{};
  std::thread m_serverThread;
  std::string m_buildWasmPath;
  int m_listenSocket{};
};
}
