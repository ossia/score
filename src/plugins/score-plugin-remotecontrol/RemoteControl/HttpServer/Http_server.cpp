//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include "Http_server.hpp"
#include <Library/LibrarySettings.hpp>

#include <cstdlib>
#include <iostream>

namespace RemoteControl
{
Http_server::Http_server() { }

Http_server::~Http_server()
{
  stop_thread();
}

// Return a reasonable mime type based on the extension of a file.
beast::string_view Http_server::mime_type(beast::string_view path)
{
  using beast::iequals;
  auto const ext = [&path]
  {
    auto const pos = path.rfind(".");
    if(pos == beast::string_view::npos)
      return beast::string_view{};
    return path.substr(pos);
  }();
  if(iequals(ext, ".htm"))  return "text/html";
  if(iequals(ext, ".html")) return "text/html";
  if(iequals(ext, ".php"))  return "text/html";
  if(iequals(ext, ".css"))  return "text/css";
  if(iequals(ext, ".txt"))  return "text/plain";
  if(iequals(ext, ".js"))   return "application/javascript";
  if(iequals(ext, ".json")) return "application/json";
  if(iequals(ext, ".xml"))  return "application/xml";
  if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
  if(iequals(ext, ".wasm")) return "application/wasm";
  if(iequals(ext, ".flv"))  return "video/x-flv";
  if(iequals(ext, ".png"))  return "image/png";
  if(iequals(ext, ".jpe"))  return "image/jpeg";
  if(iequals(ext, ".jpeg")) return "image/jpeg";
  if(iequals(ext, ".jpg"))  return "image/jpeg";
  if(iequals(ext, ".gif"))  return "image/gif";
  if(iequals(ext, ".bmp"))  return "image/bmp";
  if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
  if(iequals(ext, ".tiff")) return "image/tiff";
  if(iequals(ext, ".tif"))  return "image/tiff";
  if(iequals(ext, ".svg"))  return "image/svg+xml";
  if(iequals(ext, ".svgz")) return "image/svg+xml";
  return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string Http_server::path_cat(beast::string_view path)
{
  if (m_buildWasmPath.empty())
    return std::string(path);
  std::string result(m_buildWasmPath);
#ifdef BOOST_MSVC
  char constexpr path_separator = '\\';
  if(result.back() == path_separator)
    result.resize(result.size() - 1);
  result.append(path.data(), path.size());
  for(auto& c : result)
    if(c == '/')
      c = path_separator;
#else
  char constexpr path_separator = '/';
  if(result.back() == path_separator)
    result.resize(result.size() - 1);
  result.append(path.data(), path.size());
#endif
  return result;
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<class Body, class Allocator, class Send>
void Http_server::handle_request(
    http::request<Body, http::basic_fields<Allocator>>&& req
    , Send&& send)
{
  // Returns a bad request response
  auto const bad_request =
      [&req](beast::string_view why)
  {
    http::response<http::string_body> res{http::status::bad_request, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = std::string(why);
    res.prepare_payload();
    return res;
  };

  // Returns a not found response
  auto const not_found =
      [&req](beast::string_view target)
  {
    http::response<http::string_body> res{http::status::not_found, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "The resource '" + std::string(target) + "' was not found.<br> Go to the following address : http://ip_address:port/ossia_remote.html.";
    res.prepare_payload();
    return res;
  };

  // Returns a server error response
  auto const server_error =
      [&req](beast::string_view what)
  {
    http::response<http::string_body> res{http::status::internal_server_error, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "An error occurred: '" + std::string(what) + "'";
    res.prepare_payload();
    return res;
  };

  // Make sure we can handle the method
  if( req.method() != http::verb::get &&
     req.method() != http::verb::head)
    return send(bad_request("Unknown HTTP-method"));

  // Request path must be absolute and not contain "..".
  if( req.target().empty() ||
     req.target()[0] != '/' ||
     req.target().find("..") != beast::string_view::npos)
    return send(bad_request("Illegal request-target"));

  // Build the path to the requested file
  std::string path = path_cat(req.target());
  if(req.target().back() == '/')
    path.append("index.html");

  // Attempt to open the file
  beast::error_code ec;
  http::file_body::value_type body;

  body.open(path.c_str(), beast::file_mode::scan, ec);

  // Handle the case where the file doesn't exist
  if(ec == beast::errc::no_such_file_or_directory)
    return send(not_found(req.target()));

  // Handle an unknown error
  if(ec)
    return send(server_error(ec.message()));

  // Cache the size since we need it after the move
  auto const size = body.size();

  // Respond to HEAD request
  if(req.method() == http::verb::head)
  {
    http::response<http::empty_body>
        res{http::status::ok, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(false);
    return send(std::move(res));
  }

  // Respond to GET request
  http::response<http::file_body>
      res{std::piecewise_construct
          , std::make_tuple(std::move(body))
          , std::make_tuple(http::status::ok, req.version())};

  // Allow Cross-Origin Resource Sharing (CORS).
  res.set(http::field::access_control_allow_headers, "*");
  res.set(http::field::access_control_allow_origin, "*");
  res.set(http::field::access_control_allow_methods, "GET");

  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, mime_type(path));
  res.content_length(size);
  res.keep_alive(false);
  return send(std::move(res));
}

// Report a failure
void Http_server::fail(beast::error_code ec, char const* what)
{
  std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
void Http_server::do_session(tcp::socket& socket)
{
  bool close = false;
  beast::error_code ec;

  // This buffer is required to persist across reads
  beast::flat_buffer buffer;

  // This lambda is used to send messages
  send_lambda<tcp::socket> lambda{socket, close, ec};

  for(;;)
  {
    // Read a request
    http::request<http::string_body> req;
    http::read(socket, buffer, req, ec);
    if(ec == http::error::end_of_stream)
      break;
    if(ec)
      return Http_server::fail(ec, "read");

    // Send the response
    Http_server::handle_request(std::move(req), lambda);
    if(ec)
      return Http_server::fail(ec, "write");
    if(close)
    {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      break;
    }
  }

  // Send a TCP shutdown
  socket.shutdown(tcp::socket::shutdown_send, ec);

  // At this point the connection is closed gracefully
}

// Launch the open_server function in a thread
void Http_server::start_thread()
{
  m_serverThread = std::thread{[this] { open_server(); }};
}

void Http_server::stop_thread()
{
  if (!running()) return;

  shutdown(m_listenSocket, SHUT_RDWR);
  m_ioc.stop();
  m_serverThread.join();
}

void Http_server::set_path(const std::string& str)
{
  // FIXME : Not thread safe, but is it that bad ?
  m_buildWasmPath = str;
}

void Http_server::set_address(const std::string& str)
{
  bool is_runnig{running()};
  if (is_runnig) stop_thread();
  m_endpoint.address(net::ip::make_address(str.c_str()));
  if (is_runnig) start_thread();
}

void Http_server::set_port(unsigned short prt)
{
  bool is_runnig{running()};
  if (is_runnig) stop_thread();
  m_endpoint.port(prt);
  if (is_runnig) start_thread();
}

// Open a server using sockets
int Http_server::open_server()
{
  try
  {
    // The acceptor receives incoming connections
    tcp::acceptor acceptor{m_ioc, m_endpoint};
    m_listenSocket = acceptor.native_handle();
    for(;;)
    {
      // This will receive the new connection
      tcp::socket socket{m_ioc};

      // Block until we get a connection
      acceptor.accept(socket);

      // Launch the session, transferring ownership of the socket
      do_session(socket);
    }
  }
  catch (const std::exception& e)
  {
    std::cerr << "Error: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
}

bool Http_server::running()
{
  return m_serverThread.joinable();
}

}
