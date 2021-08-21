//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <RemoteControl/HttpServer.hpp>

//------------------------------------------------------------------------------

namespace RemoteControl
{

HttpServer::HttpServer()
{
  // m_docRoot = "/tmp";
}

HttpServer::~HttpServer()
{
     shutdown(m_listenSocket, SHUT_RDWR);
     ioc.stop();
     m_serverThread.join();
}

// Return a reasonable mime type based on the extension of a file.
beast::string_view
HttpServer::mime_type(beast::string_view path)
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
std::string
HttpServer::path_cat(
    beast::string_view base,
    beast::string_view path)
{
    if (base.empty())
        return std::string(path);
    std::string result(base);
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
template<
    class Body, class Allocator,
    class Send>
void
HttpServer::handle_request(
    beast::string_view doc_root,
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send)
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
        res.body() = "The resource '" + std::string(target) + "' was not found.y<br> Go to the following address : http://ip_address:port/remote.html.";
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
    std::string path = path_cat(doc_root, req.target());
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

    if ( req.target().find("remote.html") != std::string::npos )
    {
        // Load file
        QFile f(path.c_str());
        f.open(QIODevice::ReadOnly);
        QByteArray remote = f.readAll();

        // Write ip address in the std::string
        std::string string_remote = remote.toStdString();
        std::string::size_type position = string_remote.find("%SCORE_IP_ADDRESS%");
        std::string addr = "\"" + m_ipAddress + "\"";
        string_remote = string_remote.replace(position, 18, addr);
    }

    // Respond to HEAD request
    if(req.method() == http::verb::head)
    {
        http::response<http::empty_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(false);
        return send(std::move(res));
    }

    // Respond to GET request
    http::response<http::file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok, req.version())};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, mime_type(path));
    res.content_length(size);
    res.keep_alive(false);
    return send(std::move(res));
}

//------------------------------------------------------------------------------

// Report a failure
void
HttpServer::fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Handles an HTTP server connection
void
HttpServer::do_session(
    tcp::socket& socket,
    std::shared_ptr<std::string const> const& doc_root)
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
            return HttpServer::fail(ec, "read");

        // Send the response
        HttpServer::handle_request(*doc_root, std::move(req), lambda);
        if(ec)
            return HttpServer::fail(ec, "write");
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

//------------------------------------------------------------------------------

// Set the IP address in the remote.html file
void
HttpServer::set_ip_address(std::string address)
{
    qDebug() << "buildWasmPath :" << QString::fromStdString(m_buildWasmPath);
    std::rename((m_buildWasmPath + "remote.html").c_str(), (m_buildWasmPath + "remote.html~").c_str());

    std::ifstream old_file(m_buildWasmPath + "remote.html~");
    std::ofstream new_file(m_buildWasmPath + "remote.html");

    std::string addr = "\"" + m_ipAddress + "\"";

    for( std::string contents_of_file; std::getline(old_file, contents_of_file); ) {
      std::string::size_type position = contents_of_file.find("%SCORE_IP_ADDRESS%");
      if( position != std::string::npos )
      {
        //contents_of_file = contents_of_file.replace(position, 18, addr);
        contents_of_file = contents_of_file.replace(position, 18, "%SCORE_IP_ADDRESS%");
      }
      new_file << contents_of_file << '\n';
    }
}

//------------------------------------------------------------------------------

// Launch the open_server function in a thread
void
HttpServer::start_thread()
{
    m_serverThread = std::thread{[this] { open_server(); }};
}

//------------------------------------------------------------------------------

// Open a server using sockets
int
HttpServer::open_server()
{
    try
    {
        auto const address2 = net::ip::make_address("0.0.0.0");
        auto const port = static_cast<unsigned short>(std::atoi("8080"));
        std::string packagesPath = score::AppContext().settings<Library::Settings::Model>().getPackagesPath().toStdString();
        m_buildWasmPath = packagesPath + "/build-wasm/";
        auto const m_docRoot = std::make_shared<std::string>(m_buildWasmPath);

        bool is_ip_address_set = false;

        // The acceptor receives incoming connections
        tcp::acceptor acceptor{ioc, {address2, port}};
        m_listenSocket = acceptor.native_handle();
        for(;;)
        {
            // This will receive the new connection
            tcp::socket socket{ioc};

            // Block until we get a connection
            acceptor.accept(socket);

            // Set ip address
            if(!is_ip_address_set)
            {
                m_ipAddress = socket.local_endpoint().address().to_string();
                is_ip_address_set = true;
            }

            // Launch the session, transferring ownership of the socket
            do_session(socket, m_docRoot);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}

}
